#include <Arduino.h>
#include <LilyGoLib.h>
#include "../score-function/score-function.h"

LeadershipEvaluator evaluator;

void setup() {
  Serial.begin(115200);
  delay(10000);

  // Initialize the watch (includes power chip, screen, etc.)
  watch.begin();
  Serial.println("✅ Watch initialized");

  // Example soldier→commander pairs
  std::vector<std::pair<LatLon, LatLon>> links = {
    {{32.0853, 34.7818}, {32.0856, 34.7821}},
    {{32.0854, 34.7819}, {32.0856, 34.7821}},
    {{32.0855, 34.7820}, {32.0856, 34.7821}},
  };

  // Print leadership score and battery info
  evaluator.printScore(links);
}

void loop() {
  // Do nothing — could refresh score here if needed
  delay(5000);
}
