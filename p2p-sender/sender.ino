#include <LilyGoLib.h>
#include <LV_Helper.h>

// === LoRa Setup ===
SX1262 lora = newModule();

// === Coordinates ===
struct GPSCoord {
  float lat1;
  float lon1;
  float lat2;
  float lon2;
};

GPSCoord coords[] = {
  {32.08530, 34.78180, 32.08539,   34.78180},
  {32.08530, 34.78180, 32.0853278, 34.7819010},
  {32.08530, 34.78180, 32.0852272, 34.7818623},
  {32.08530, 34.78180, 32.0852272, 34.7817377},
  {32.08530, 34.78180, 32.0853278, 34.7816990}
};

const int coordCount = sizeof(coords) / sizeof(coords[0]);
int currentIndex = 0;
bool alreadyTouched = false;
int transmissionState = RADIOLIB_ERR_NONE;

// === Function Prototypes ===
void setupLoRa();
void sendCoordinate(int index);
bool screenTouched();

volatile bool transmittedFlag = false;

ICACHE_RAM_ATTR void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  delay(1000);
  watch.begin(&Serial);
  beginLvglHelper();

  setupLoRa();

  Serial.println("Touch screen to send next coordinate.");
}

// === Loop ===
void loop() {
   

  if (transmittedFlag) {
    transmittedFlag = false;

    if (transmissionState == RADIOLIB_ERR_NONE) {
      Serial.println(F("transmission finished!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);
    }

    lora.finishTransmit();

  }

  if (screenTouched()) {
    sendCoordinate(currentIndex++);
    if (currentIndex >= coordCount)
    {
      currentIndex = 0;
    }
  }

  lv_task_handler();
  delay(10);
}

// === LoRa Setup ===
void setupLoRa() {
  Serial.print(F("[LoRa] Initializing... "));

  int state = lora.begin();

  if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
  } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true);
  }


  if (lora.setFrequency(433.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true);
  }

  lora.setDio1Action(setFlag);

  Serial.print(F("[SX1262] Sending first packet ... "));
}

// === Touch Detection ===
bool screenTouched() {
  lv_point_t point; //define a point for screen (x,y)
  lv_indev_t *indev = lv_indev_get_next(NULL); //retrive last place touched on screen
  if (!indev) return false;

#if LV_VERSION_CHECK(9,0,0)
  bool isPressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
#else
  bool isPressed = indev->proc.state == LV_INDEV_STATE_PRESSED;
#endif

  if (isPressed && !alreadyTouched) {
    alreadyTouched = true;
    lv_indev_get_point(indev, &point);
    Serial.print("📍 Touch detected at x=");
    Serial.print(point.x);
    Serial.print(" y=");
    Serial.println(point.y);
    return true;
  }

  if (!isPressed) {
    alreadyTouched = false;
  }

  return false;
}

// === Send Coordinate ===
void sendCoordinate(int index) {
  char message[64];
  snprintf(message, sizeof(message),
           "%.5f,%.5f;%.5f,%.5f",
           coords[index].lat1, coords[index].lon1,
           coords[index].lat2, coords[index].lon2);

  Serial.print(F("[LoRa] Sending message: "));
  Serial.println(message);

  int state = lora.startTransmit(message);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("✅ Message sent successfully!"));
  } else {
    Serial.print(F("❌ Send failed, code "));
    Serial.println(state);
  }

  if (index == coordCount - 1) {
    Serial.println(F("✅ All coordinates sent. Further taps are ignored."));
  }
}