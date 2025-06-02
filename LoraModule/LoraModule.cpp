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

  this->lastFrequencyCheck = millis();
  return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::sendData(const char* data, bool interrupt)
{
  Serial.println("sendData");
  if (!tryStartOp(Op::Transmit)) 
  {
    return RADIOLIB_ERR_CHANNEL_BUSY;
  }
  Serial.println("moved in sendData");

  int16_t status = interrupt
      ? loraDevice.startTransmit(data)
      : loraDevice.transmit(data);
  Serial.printf("transmission status: %d\n", status);
  if (status != RADIOLIB_ERR_NONE) {
    currentOp.store(Op::None, std::memory_order_release);
    return status;
  }

  return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::readData()
{
  Serial.println("ReadData called");
  if (!tryStartOp(Op::Receive))
  {
    return RADIOLIB_ERR_NONE;
  }

  Serial.println("moved in ReadData");
  int16_t status = loraDevice.startReceive();
  if (status != RADIOLIB_ERR_NONE) 
  {
    currentOp.store(Op::None, std::memory_order_release);
  }

  return status;
}

int16_t LoraModule::sendFile(const uint8_t* data, size_t length, size_t chunkSize)
{
  if (!data || length == 0) 
  {
    return RADIOLIB_ERR_NONE;
  }

  Serial.println("sendFile");

  while (!tryStartOp(Op::FileTx)) 
  {
    delay(1);
  }

  this->loraDevice.setDio1Action(nullptr);

  String initFrame = String(kFileInitTag) + ":" +
                     String(length)       + ":" +
                     String(chunkSize);

  int16_t status = loraDevice.transmit(initFrame.c_str());
  if (status != RADIOLIB_ERR_NONE) 
  {
    currentOp.store(Op::None, std::memory_order_release);
    loraDevice.setDio1Action(dio1ISR);
    return status;
  }

  delay(90);

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

    if (status != RADIOLIB_ERR_NONE) 
    {
      loraDevice.setDio1Action(dio1ISR);
      break;
    }

    delay(50);
  }

  Serial.println("RESETTING ACTION");
  loraDevice.setDio1Action(dio1ISR);

  if (status == RADIOLIB_ERR_NONE) 
  {
    status = loraDevice.transmit(kFileEndTag);
  }

  while (!opFinished.load(std::memory_order_acquire)) 
  {
    delay(1);
  }

  Serial.println("FINISHED FILE");
  handleCompletedOperation();
  
  return status;
}

void LoraModule::onLoraFileDataReceived(const uint8_t* pkt, size_t len)
{
    if (len == 0)
    {
      return;
    }

    Serial.printf("📦 Incoming packet: %.*s\n", len, pkt);
    

    if (memcmp(pkt, kFileInitTag, strlen(kFileInitTag)) == 0) 
    {
      
      if (!tryStartOp(Op::FileRx))
      {
        return;
      }

      size_t totalSize = 0;
      this->transferChunkSize = 0;

      if (sscanf((const char*)pkt, "FILE_INIT:%zu:%zu", &totalSize, &this->transferChunkSize) == 2) 
      {
        this->expectedChunks = (totalSize + this->transferChunkSize - 1) / this->transferChunkSize;
        this->fileBuffer.clear();
        this->fileBuffer.resize(totalSize);
        this->receivedChunks = 0;

        Serial.printf("📥 INIT: totalSize=%zu, chunkSize=%zu, expectedChunks=%u\n", totalSize,
          this->transferChunkSize, this->expectedChunks);
      } 
      else
      {
        Serial.println("❌ INIT frame parse failed!");
      }

      return;
    }

    if (memcmp(pkt, kFileEndTag, strlen(kFileEndTag)) == 0) 
    {
        if (this->onFileReceived && this->receivedChunks == this->expectedChunks) 
        {
          Serial.printf("✅ File complete (%u chunks)\n", this->receivedChunks);
          this->currentOp.store(Op::None, std::memory_order_release);
          this->onFileReceived(this->fileBuffer.data(), this->fileBuffer.size());
          Serial.println("returning from function");
          return;
        }
        else 
        {
          Serial.printf("❌ Incomplete file (%u/%u chunks)\n", this->receivedChunks, this->expectedChunks);
        }
        return;
    }

    if (pkt[0] == 0xAB && len >= 4) 
    {
      uint16_t chunkIndex = (pkt[1] << 8) | pkt[2];
      uint8_t chunkLen = pkt[3];
      size_t offset = chunkIndex * this->transferChunkSize;

      if (offset + chunkLen > this->fileBuffer.size()) {
          Serial.printf("❌ Chunk %u out of bounds (offset %zu + len %u > buffer %zu)\n",
                        chunkIndex, offset, chunkLen, this->fileBuffer.size());

          return;
      }

      memcpy(&this->fileBuffer[offset], pkt + 4, chunkLen);
      this->receivedChunks++;

      Serial.printf("🧩 Chunk #%u received: offset=%zu, len=%u, totalChunks=%u/%u\n",
                    chunkIndex, offset, chunkLen, this->receivedChunks, this->expectedChunks);
    }
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
  if (!opFinished.load(std::memory_order_acquire)) 
  {
    return;
  }

  Serial.println("handleCompletedOperation");

  Op op = currentOp.load(std::memory_order_acquire);

  if(op != Op::FileRx)
  {
    currentOp.store(Op::None, std::memory_order_release);
  }

  opFinished.store(false, std::memory_order_release);

  Serial.println("moved in handleCompletedOperation");

  if (op == Op::Receive)
  {
    Serial.println("RECEIVE FINISHED");
  }
  else if(op == Op::Transmit)
  {
    Serial.println("TRANSMIT FINISHED");
  }
  else if(op == Op::FileRx)
  {
    Serial.println("FileRx FINISHED");
  }
  else if(op == Op::FileTx)
  {
    Serial.println("FileTx FINISHED");
  }

  if (op == Op::Receive || op == Op::FileRx)
  {
    uint8_t buf[512];
    size_t len = sizeof(buf);
    if (loraDevice.readData(buf, len) == RADIOLIB_ERR_NONE) {
      size_t pktLen = loraDevice.getPacketLength();
      if(pktLen > 0)
      {
        Serial.println(reinterpret_cast<const char*>(buf));
      }
      if (onReadData) 
      {
        onReadData(buf, pktLen);
      }
    }

    if(op == Op::FileRx)
    {
      loraDevice.startReceive();
    }
  }
}

void LoraModule::syncFrequency(const FHFModule* module)
{
  uint64_t currentFreqCheck = millis();

  if(currentFreqCheck - this->lastFrequencyCheck >= 1000 && !isBusy())
  {
    float currentFreq = module->currentFrequency();

    if(std::fabs(currentFreq - this->lastFrequencyCheck) > 1e-9)
    {
      this->setFrequency(currentFreq);
      this->lastFrequencyCheck = currentFreqCheck;
    }
  }
}