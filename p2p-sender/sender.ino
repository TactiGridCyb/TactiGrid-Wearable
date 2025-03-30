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
  {32.0853, 34.7818, 32.0855, 34.7820},
  {32.0854, 34.7819, 32.0856, 34.7821},
  {32.0855, 34.7820, 32.0857, 34.7822},
  {32.0856, 34.7821, 32.0858, 34.7823},
  {32.0857, 34.7822, 32.0859, 34.7824}
};

const int coordCount = sizeof(coords) / sizeof(coords[0]);
int currentIndex = 0;
bool alreadyTouched = false;

// === Function Prototypes ===
void setupLoRa();
void sendCoordinate(int index);
bool screenTouched();

// === Setup ===
void setup() {
  Serial.begin(115200);
  delay(1000);
  watch.begin();
  beginLvglHelper();

  setupLoRa();

  Serial.println("Touch screen to send next coordinate.");
}

// === Loop ===
void loop() {
  lv_task_handler();  // Update LVGL UI

  if (screenTouched() && currentIndex < coordCount) { //calling to screen event and waiting for touch
    sendCoordinate(currentIndex++);
  }

  delay(10);  // CPU-friendly delay
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

  lora.setFrequency(868.0);
  lora.setOutputPower(10);
  lora.setSpreadingFactor(7);
  lora.setBandwidth(125.0);
  lora.setCodingRate(5);
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
           "%.2f,%.2f;%.2f,%.2f",
           coords[index].lat1, coords[index].lon1,
           coords[index].lat2, coords[index].lon2);

  Serial.print(F("[LoRa] Sending message: "));
  Serial.println(message);

  int state = lora.transmit(message);
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
