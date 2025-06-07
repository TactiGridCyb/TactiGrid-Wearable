#pragma once

#include <LilyGoLib.h>
#include <FFat.h>
#include <FFatHelper.h>
#include <vector>

class ShamirHelper {
public:
  // Splits the file at inputPath into nShares, requiring threshold to reconstruct.
  // Returns a vector of share file paths.
  static bool splitFile(const char* inputPath, int nShares, const std::vector<uint8_t>& shareIds,
  std::vector<String>& sharePaths);

  // Reconstructs the original file from at least 'threshold' sharePaths.
  // Writes the result to outputPath. Returns true on success.
  static bool reconstructFile(const std::vector<String>& sharePaths, const char* outputPath);

  static void setThreshold(uint8_t minThreshold);

  static uint8_t getThreshold();
private:
  const static int PRIME = 257;
  static uint8_t minThreshold;

  static uint16_t evalPolynomial(uint8_t x, uint8_t secret, uint16_t randomCoeff, int prime);
  static int modInverse(int a, int m);
  static int lagrangeInterpolation(int atX, const std::vector<int>& xVals, const std::vector<int>& yVals, int prime);

};
