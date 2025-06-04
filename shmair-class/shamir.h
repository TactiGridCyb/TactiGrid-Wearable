#pragma once

#include <Arduino.h>
#include <LilyGoLib.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FFat.h>
#include <lvgl.h>
#include <LV_Helper.h>
#include <FFat.h>
#include <vector>

class ShamirSecretSharing {
public:
  ShamirSecretSharing(int prime = 257);

  // Splits the file at inputPath into nShares, requiring threshold to reconstruct.
  // Returns a vector of share file paths.
  std::vector<String> splitFile(const char* inputPath, int nShares, int threshold);

  // Reconstructs the original file from at least 'threshold' sharePaths.
  // Writes the result to outputPath. Returns true on success.
  bool reconstructFile(const std::vector<String>& sharePaths, int threshold, const char* outputPath);

private:
  const int PRIME;

  inline uint8_t evalPolynomial(uint8_t x, uint8_t secret, uint8_t randomCoeff);
  int modInverse(int a, int m);
  int lagrangeInterpolation(int atX, const std::vector<int>& xVals, const std::vector<int>& yVals);
};
