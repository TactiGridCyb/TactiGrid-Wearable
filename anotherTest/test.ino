#include <LilyGoLib.h>
#include <sodium.h>

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  delay(500); 
  Serial.println("About to call sodium_init()");

  int ret = sodium_init();
  Serial.printf("sodium_init() returned %d\n", ret);
  Serial.println(sodium_version_string());
}

void loop() {
  // nothing else
  delay(1000);
}