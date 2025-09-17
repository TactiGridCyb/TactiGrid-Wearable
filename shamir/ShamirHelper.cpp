#include "ShamirHelper.h"

uint8_t ShamirHelper::minThreshold = 2;

//=============================================================================
// splitFile:
//  - inputPath: path to the file to split (e.g., "/config.json")
//  - nShares: total number of shares to create (must be >= threshold and >= 2)
//  - threshold: minimum number of shares needed to reconstruct
// Returns a vector of Strings containing the file paths of the generated share files.
// Each share file is named inputPath + ".share" + index.
//=============================================================================
bool ShamirHelper::splitFile(const char* inputPath, int nShares, const std::vector<uint8_t>& shareIds,
  std::vector<String>& sharePaths) {
  sharePaths.clear();  // ensure it's empty before starting

  if (nShares < ShamirHelper::minThreshold || ShamirHelper::minThreshold < 2) 
  {
    Serial.println("❌ Invalid parameters: nShares must be >= threshold >= 2.");
    return false;
  }

  File inFile = FFat.open(inputPath, FILE_READ);
  if (!inFile) {
    Serial.print("❌ Failed to open input file: ");
    Serial.println(inputPath);
    return false;
  }

  Serial.println("📄 Content of the input file:");
  while (inFile.available()) {
      char ch = inFile.read();
      Serial.print(ch);
  }
  
  Serial.println();
  inFile.seek(0);


  std::vector<File> shareFiles;
  shareFiles.reserve(nShares);
  for (int i = 0; i < nShares; i++) {
    uint8_t id = shareIds[i];
    String spath = "/share_" + String(id) + ".txt";
    Serial.printf("Creating spath %s\n", spath);

    File sf = FFat.open(spath.c_str(), FILE_WRITE);
    if (!sf) {
      Serial.print("❌ Failed to create share file: ");
      Serial.println(spath);
      for (File& f : shareFiles) f.close();
      inFile.close();
      return false;
    }
    shareFiles.push_back(std::move(sf));
    sharePaths.push_back(spath);
  }

  Serial.println("-------------------------------");
  while (inFile.available()) {
    uint8_t secretByte = inFile.read();
    uint16_t randomCoeff = random(1, ShamirHelper::PRIME);
    for (int i = 0; i < nShares; i++) {

        uint16_t x = shareIds[i] % ShamirHelper::PRIME;
        if (x == 0) x = 1;
        uint16_t y = evalPolynomial(x, secretByte, randomCoeff, ShamirHelper::PRIME);

        shareFiles[i].print((int)x);
        shareFiles[i].print(',');
        shareFiles[i].println((int)y);

        // Serial.print("(");
        // Serial.print(i);
        // Serial.print(",");
        // Serial.print(y);
        // Serial.print(") ");
    }
  }

  for (File& sf : shareFiles) sf.close();

  inFile.close();

  Serial.print("✅ Split complete: ");
  Serial.print(nShares);
  Serial.print(" share files created (threshold = ");
  Serial.print(ShamirHelper::minThreshold);
  Serial.println(").");

  return true;
}

//=============================================================================
// reconstructFile:
//  - sharePaths: vector of at least 'threshold' file paths, each containing lines "x,y"
//  - threshold: the minimum number of shares needed; must match what was used in splitFile
//  - outputPath: path where the recovered file will be written (e.g., "/recovered.json")
// Returns true on success, false on failure.
//=============================================================================
bool ShamirHelper::reconstructFile(const std::vector<String>& sharePaths, const char* outputPath) {
  int availableShares = sharePaths.size();
  if (availableShares < ShamirHelper::minThreshold) {
    Serial.println("❌ Not enough shares provided to reconstruct.");
    return false;
  }

  std::vector<File> shareFiles;
  shareFiles.reserve(availableShares);
  std::vector<int> xValues(availableShares, 0);

  for (int idx = 0; idx < availableShares; idx++) {
    shareFiles.emplace_back(FFat.open(sharePaths[idx].c_str(), FILE_READ));
    File &sf = shareFiles.back();
    if (!sf) {
      Serial.print("❌ Failed to open share file: ");
      Serial.println(sharePaths[idx]);
      for (File& f : shareFiles) f.close();
      return false;
    }
    String firstLine = sf.readStringUntil('\n');
    sf.seek(0);
    int commaIdx = firstLine.indexOf(',');
    if (commaIdx < 1) {
      Serial.print("❌ Invalid share file format: ");
      Serial.println(sharePaths[idx]);
      sf.close();
      for (File& f : shareFiles) f.close();
      return false;
    }

    xValues[idx] = firstLine.substring(0, commaIdx).toInt();
  }

  for (int i = 0; i < availableShares; ++i) 
  {
    xValues[i] %= ShamirHelper::PRIME;
  }

  int byteCount = 0;
  File& firstShare = shareFiles[0];
  while (firstShare.available()) {
    firstShare.readStringUntil('\n');
    byteCount++;
  }
  firstShare.seek(0);

  File outFile = FFat.open(outputPath, FILE_WRITE);
  if (!outFile) {
    Serial.print("❌ Failed to create output file: ");
    Serial.println(outputPath);
    for (File& sf : shareFiles) sf.close();
    return false;
  }

  std::vector<int> yValues(ShamirHelper::minThreshold);
  std::vector<int> xSubset(ShamirHelper::minThreshold);
  std::vector<uint8_t> secret(byteCount);


  for (int byteIdx = 0; byteIdx < byteCount; byteIdx++) {
    for (int s = 0; s < ShamirHelper::minThreshold; s++) {
      String line = shareFiles[s].readStringUntil('\n');
      int commaPos = line.indexOf(',');
      if (commaPos < 1) {
        Serial.println("❌ Malformed share line during reconstruction.");
        outFile.close();
        for (File& sf : shareFiles) sf.close();
        return false;
      }

      xSubset[s] = xValues[s] % ShamirHelper::PRIME;
      if (xSubset[s] == 0) xSubset[s] = 1;
      yValues[s] = line.substring(commaPos + 1).toInt() % ShamirHelper::PRIME;

    }

    int recovered = lagrangeInterpolation(0, xSubset, yValues, ShamirHelper::PRIME) % ShamirHelper::PRIME;
    secret[byteIdx] = (uint8_t)recovered;
  }

  outFile.write(secret.data(), secret.size());

  outFile.close();
  for (File& sf : shareFiles) sf.close();

  Serial.print("✅ Reconstruction complete: ");
  Serial.print(byteCount);
  Serial.print(" bytes written to ");
  Serial.println(outputPath);
  return true;
}

//=============================================================================
// evalPolynomial: f(x) = (secret + randomCoeff * x) mod PRIME
//=============================================================================
uint16_t ShamirHelper::evalPolynomial(uint16_t x, uint8_t secret, uint16_t randomCoeff, int prime) 
{

  uint32_t prod = (uint32_t)randomCoeff * (uint32_t)(x % prime);
  return (uint16_t)((secret + (prod % prime)) % prime);
}

static inline int modp(int a, int p)
{ 
  a %= p;

  if (a < 0)
  {
    a += p;
  }

  return a; 

}

//=============================================================================
// modInverse: Extended Euclidean Algorithm to find modular inverse of 'a' modulo 'm'
// Returns inverse in [0..m-1], or 0 if no inverse (should not happen if m is prime and a != 0)
//=============================================================================

int ShamirHelper::modInverse(int a, int p) {
  a = modp(a, p);
  if (a == 0) return 0;
  int t = 0, newt = 1;
  int r = p, newr = a;

  while (newr != 0) 
  {
    int q = r / newr;
    int tmp = t - q * newt;
    t = newt;
    newt = tmp;
    tmp = r - q * newr;
    r = newr; 
    newr = tmp;
  }

  if (r != 1) return 0;
  if (t < 0) t += p;
  return t;
}

//=============================================================================
// lagrangeInterpolation: Given vectors xVals[0..k-1], yVals[0..k-1], compute f(atX)
// over GF(PRIME), where f is a polynomial of degree < k that matches the given points.
// Typically atX = 0 to recover the secret.
//=============================================================================
int ShamirHelper::lagrangeInterpolation(int atX, const std::vector<int>& xVals, const std::vector<int>& yVals, int prime) {
  int k = xVals.size();
  long result = 0;

  for (int i = 0; i < k; i++) {
    long numerator = 1;
    long denominator = 1;
    for (int j = 0; j < k; j++) {
      if (j == i) continue;
      numerator = (numerator * (atX - xVals[j] + prime)) % prime;
      denominator = (denominator * (xVals[i] - xVals[j] + prime)) % prime;
    }
    int invDenom = modInverse((int)(denominator % prime), prime);
    long term = ((long)yVals[i] * numerator % prime) * invDenom % prime;
    result = (result + term) % prime;
  }

  return (int)result;
}


void ShamirHelper::setThreshold(uint8_t minThreshold)
{
  ShamirHelper::minThreshold = minThreshold;
}

uint8_t ShamirHelper::getThreshold()
{
  return ShamirHelper::minThreshold;
}