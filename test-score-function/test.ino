#include <Arduino.h>
#include <XPowersLib.h>           // XPowers class for battery reads
#include "score-function.h"       // Our custom header

// Define the global XPowers instance:
//XPowers pwr;

// Create one LeadershipEvaluator with default weights (0.5,0.5) & maxDist=1000m:
LeadershipEvaluator leaderEval;

void setup() {
  Serial.begin(115200);
  delay(5000);

  Serial.println("inside:");
  // 1) Initialize XPowersLib (AXP192/AXP202 chip) & enable ADC:
  //pwr.begin();            // This talks over I²C to the AXP chip
  leaderEval.begin();     // This calls pwr.enableBattVoltage(true);

  // 2) Example soldier coordinates (3 soldiers), and 1 commander:
  std::vector<LatLon> soldiers = {
    {32.0853, 34.7818},
    {32.0854, 34.7819},
    {32.0855, 34.7820}
  };
  LatLon commander = {32.0856, 34.7821};

  // 3) Print initial battery + leadership score to screen:
  leaderEval.printScoreToScreen(soldiers, commander);

  // 4) If you want to see the “shouldReplace” logic:
  if (leaderEval.shouldReplace(soldiers, commander)) {
    Serial.println("🏳️  Replace Commander (score < threshold)");
  } else {
    Serial.println("✅ Commander OK (score ≥ threshold)");
  }
}

void loop() {
  // You could update the soldier list / commander coords in real code,
  // then call printScoreToScreen(...) again.
  delay(500);
}
