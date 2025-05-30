#include "LoraModule.h"
#include <Arduino.h>

#define RADIOLIB_ERR_CHANNEL_BUSY -1000

LoraModule* LoraModule::instance = nullptr;

LoraModule::LoraModule(float frequency)
  : freq(frequency)
{
  instance = this;
}

LoraModule::~LoraModule()
{
  loraDevice.finishTransmit();
  instance = nullptr;
}

void IRAM_ATTR LoraModule::dio1ISR()
{
  if (instance) {
    instance->notifyOpFinished();
  }
}

bool LoraModule::tryStartOp(Op desired)
{
  Op none = Op::None;
  if (!currentOp.compare_exchange_strong(none, desired,
                                         std::memory_order_acq_rel)) {
    return false;
  }

  opFinished.store(false, std::memory_order_release);
  return true;
}

void LoraModule::notifyOpFinished()
{
  opFinished.store(true, std::memory_order_release);
}


int16_t LoraModule::setup(bool transmissionMode_)
{
  int16_t state = loraDevice.begin();
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  loraDevice.setFrequency(freq);
  loraDevice.setBandwidth(125.0);
  loraDevice.setSpreadingFactor(8);
  loraDevice.setCodingRate(6);
  loraDevice.setSyncWord(0xAB);
  loraDevice.setOutputPower(22);
  loraDevice.setCurrentLimit(140);
  loraDevice.setPreambleLength(12);
  loraDevice.setCRC(true);
  loraDevice.setTCXO(3.0);
  loraDevice.setDio2AsRfSwitch();

  loraDevice.setDio1Action(dio1ISR);

  transmissionMode = transmissionMode_;
  return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::sendData(const char* data, bool interrupt)
{
  if (!tryStartOp(Op::Transmit)) {
    return RADIOLIB_ERR_CHANNEL_BUSY;
  }

  int16_t status = interrupt
      ? loraDevice.startTransmit(data)
      : loraDevice.transmit(data);

  if (status != RADIOLIB_ERR_NONE) {
    currentOp.store(Op::None, std::memory_order_release);
    return status;
  }

  return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::readData()
{
  if (!tryStartOp(Op::Receive)) {
    return RADIOLIB_ERR_NONE;
  }

  int16_t status = loraDevice.startReceive();
  if (status != RADIOLIB_ERR_NONE) {
    currentOp.store(Op::None, std::memory_order_release);
  }
  return status;
}

int16_t LoraModule::sendFile(const uint8_t* data, size_t length, size_t chunkSize)
{
  if (!data || length == 0) {
    return RADIOLIB_ERR_NONE;
  }

  if (!tryStartOp(Op::FileTx)) {
    return RADIOLIB_ERR_CHANNEL_BUSY;
  }

  String initFrame = String(kFileInitTag) + ":" +
                     String(length)       + ":" +
                     String(chunkSize);
  int16_t status = loraDevice.transmit(initFrame.c_str());
  if (status != RADIOLIB_ERR_NONE) {
    currentOp.store(Op::None, std::memory_order_release);
    return status;
  }
  delay(100);

  uint16_t totalChunks = (length + chunkSize - 1) / chunkSize;
  for (uint16_t i = 0; i < totalChunks; ++i) {
    size_t offset = i * chunkSize;
    size_t segLen = min(chunkSize, length - offset);
    std::vector<uint8_t> packet(4 + segLen);
    packet[0] = 0xAB;
    packet[1] = (i >> 8) & 0xFF;
    packet[2] = (i & 0xFF);
    packet[3] = segLen;
    memcpy(packet.data() + 4, data + offset, segLen);
    status = loraDevice.transmit(packet.data(), packet.size());
    if (status != RADIOLIB_ERR_NONE) {
      break;
    }
    delay(50);
  }

  if (status == RADIOLIB_ERR_NONE) {
    status = loraDevice.transmit(kFileEndTag);
  }

  while (!opFinished.load(std::memory_order_acquire)) {
    delay(1);
  }
  handleCompletedOperation();
  return status;
}

int16_t LoraModule::setFrequency(float newFreq)
{
  if (isBusy()) {
    return freq;
  }
  freq = newFreq;
  loraDevice.setFrequency(freq);
  return freq;
}

void LoraModule::switchToTransmitterMode()
{
  transmissionMode = true;
}

void LoraModule::switchToReceiverMode()
{
  transmissionMode = false;
}

bool LoraModule::isChannelFree()
{
  loraDevice.setDio1Action(nullptr);
  
  int16_t result = loraDevice.scanChannel();

  loraDevice.setDio1Action(dio1ISR);

  return (result == RADIOLIB_CHANNEL_FREE);
}

void LoraModule::cancelReceive() {
  loraDevice.standby();
  currentOp.store(Op::None, std::memory_order_release);
  opFinished.store(false, std::memory_order_release);
}

bool LoraModule::canTransmit()
{
  return !isBusy() && isChannelFree();
}

bool LoraModule::isBusy() const
{
  return (currentOp.load(std::memory_order_acquire) != Op::None);
}

void LoraModule::handleCompletedOperation()
{
  if (!opFinished.load(std::memory_order_acquire)) {
    return;
  }

  Serial.println("handleCompletedOperation");

  Op op = currentOp.load(std::memory_order_acquire);
  currentOp.store(Op::None, std::memory_order_release);
  opFinished.store(false, std::memory_order_release);

  Serial.println("moved in handleCompletedOperation");

  if (op == Op::Receive) {
    uint8_t buf[512];
    size_t len = sizeof(buf);
    if (loraDevice.readData(buf, len) == RADIOLIB_ERR_NONE) {
      size_t pktLen = loraDevice.getPacketLength();
      if(pktLen > 0)
      {
        Serial.println(reinterpret_cast<const char*>(buf));
      }
      if (onReadData) {
        onReadData(buf, pktLen);
      }
    }

    loraDevice.startReceive();
  }
}
