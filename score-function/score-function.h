#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Normally, you’d include XPowersLib and declare “extern XPowers pwr;” here.
// But for a static stub, we’ll comment out those lines and return fixed values.
//
// #include <XPowers.h>       // (Would pull in the real XPowers class)
// extern XPowers pwr;         // (Would declare the global XPowers instance)
// 
// #define AXP202_BATT_VOL_ADC1 0x80  // (unused in stub)
// #define AXP202_BATT_CUR_ADC1 0x40  // (unused in stub)
// -----------------------------------------------------------------------------

#include <vector>

// If you’re using LVGL later, you’d do:
// #include <lvgl.h>

#ifdef LVGL_H
  #include <lvgl.h>
#endif

//--------------------------------------------------------------------------------
// Simple struct for a single (lat, lon) pair (still defined for completeness)
struct LatLon {
  float lat;
  float lon;
};

//--------------------------------------------------------------------------------
// LeadershipEvaluator (static stub version)
//   • All real hardware calls are commented out.
//   • getBatteryPct() returns a fixed 50%.
//   • getBatteryVolt() returns 3.70 V.
//   • calculateScore(...) returns a fixed 0.5.
//   • shouldReplace(...) returns (score < 0.5) → will be false here.
//   • printScoreToScreen(...) prints static text.
//--------------------------------------------------------------------------------
class LeadershipEvaluator {
public:
  LeadershipEvaluator(float maxDistMeters     = 1000.0f,
                      float distWeight       = 0.5f,
                      float battWeight       = 0.5f,
                      float replaceThreshold = 0.5f)
    : _maxDist(maxDistMeters)
    , _wDist(distWeight)
    , _wBatt(battWeight)
    , _replaceThreshold(replaceThreshold)
  {}

  // Call this in setup() if you need to initialize real hardware.
  // In the stub version, it does nothing.
  void begin() {
    // Real code would do:
    //   pwr.begin();
    //   pwr.enableBattVoltage(true);
    //
    // Stub: do nothing.
  }

  // Returns a fixed battery percentage (0..100). Always returns 50%.
  uint8_t getBatteryPct() const {
    return 50;  // Stub: pretend battery is always 50%
  }

  // Returns a fixed battery voltage in volts (e.g. 3.70 V).
  float getBatteryVolt() const {
    return 3.70f;  // Stub: pretend battery is always 3.70 V
  }

  // Compute a score from soldiers[] + commander.
  // Stubbed to always return 0.5.
  float calculateScore(const std::vector<LatLon>& soldiers,
                       const LatLon&           commander) const
  {
    // Real code would do:
    //   1) Compute average distance (clamped at _maxDist)
    //   2) Normalize that distance → normDist
    //   3) Read real battery % → battPct
    //   4) return _wDist*(1 - normDist) + _wBatt*battPct
    //
    // Stub: ignore inputs and return a fixed value:
    (void)soldiers;   // suppress “unused parameter” warning
    (void)commander;  // suppress “unused parameter” warning
    return 0.50f;     // Stub: always 0.50
  }

  // In a real build, “shouldReplace” would check if score < threshold.
  // Here, since calculateScore() is 0.5 and threshold is 0.5, it returns false.
  bool shouldReplace(const std::vector<LatLon>& soldiers,
                     const LatLon&           commander) const
  {
    // Real code: 
    //   return (calculateScore(soldiers, commander) < _replaceThreshold);

    // Stub version:
    (void)soldiers;
    (void)commander;
    return false;
  }

  // Print the (stubbed) score and battery info.
  // If LVGL is enabled, it draws a label; otherwise it prints over Serial.
  void printScoreToScreen(const std::vector<LatLon>& soldiers,
                          const LatLon&           commander) const
  {
    float score = calculateScore(soldiers, commander);
    uint8_t batt  = getBatteryPct();
    float volts   = getBatteryVolt();

    char buf[64];
    // Format: “Score: 0.500\nBatt: 50% (3.70V)”
    snprintf(buf, sizeof(buf),
             "Score: %.3f\nBatt: %d%% (%.2fV)",
             score, batt, volts);

    #ifdef LVGL_H
      // Real code would do something like:
      //   lv_obj_t* label = lv_label_create(lv_scr_act());
      //   lv_label_set_text(label, buf);
      //   lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
      //
      // Stub: do nothing or simulate.
    #else
      Serial.println(buf);
    #endif
  }

private:
  // In the real code, haversineDistance(...) computes distance (meters).
  // Stub: either comment out entirely or return 0. For now, we comment out:
  //
  // float haversineDistance(float lat1, float lon1,
  //                         float lat2, float lon2) const
  // {
  //   // Real implementation omitted in stub.
  //   return 0.0f;
  // }

  float _maxDist;           // (unused in stub)
  float _wDist;             // (unused in stub)
  float _wBatt;             // (unused in stub)
  float _replaceThreshold;  // (unused in stub)
};
