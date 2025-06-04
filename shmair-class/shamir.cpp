#include "shamir.h"

ShamirSecretSharing::ShamirSecretSharing(int prime) : PRIME(prime) {}

//=============================================================================
// splitFile:
//  - inputPath: path to the file to split (e.g., "/config.json")
//  - nShares: total number of shares to create (must be >= threshold and >= 2)
//  - threshold: minimum number of shares needed to reconstruct
// Returns a vector of Strings containing the file paths of the generated share files.
// Each share file is named inputPath + ".share" + index.
//=============================================================================
std::vector<String> ShamirSecretSharing::splitFile(const char* inputPath, int nShares, int threshold) {
  std::vector<String> sharePaths;

  // Validate parameters
  if (nShares < threshold || threshold < 2) {
    Serial.println("❌ Invalid parameters: nShares must be >= threshold >= 2.");
    return sharePaths;
  }

  // Open the input file to read raw bytes
  File inFile = FFat.open(inputPath, FILE_READ);
  if (!inFile) {
    Serial.print("❌ Failed to open input file: ");
    Serial.println(inputPath);
    return sharePaths;
  }

  // Prepare share file handles
  std::vector<File> shareFiles;
  shareFiles.reserve(nShares);
  for (int i = 1; i <= nShares; i++) {
    // Construct each share file name: e.g. "/config.json.share1"
    String spath = String(inputPath) + ".share" + String(i);
    File sf = FFat.open(spath.c_str(), FILE_WRITE);
    if (!sf) {
      Serial.print("❌ Failed to create share file: ");
      Serial.println(spath);
      // Close any already‐opened files
      for (File& f : shareFiles) {
        f.close();
      }
      inFile.close();
      return sharePaths;
    }
    shareFiles.push_back(sf);
    sharePaths.push_back(spath);
  }

  // Read byte by byte from the input file
  while (inFile.available()) {
    uint8_t secretByte = inFile.read();           // s = original data byte (0–255)
    uint8_t randomCoeff = random(1, PRIME);       // r = random value in [1, PRIME-1]

    // For each share index i (1..nShares), evaluate f(i) = s + r*i (mod PRIME), then write "i,y\n"
    for (int i = 1; i <= nShares; i++) {
      uint8_t y = evalPolynomial(i, secretByte, randomCoeff);
      // Format: "i,y\n"
      shareFiles[i - 1].print(String(i) + "," + String(y) + "\n");
    }
  }

  // Close all share files and the input file
  for (File& sf : shareFiles) {
    sf.close();
  }
  inFile.close();

  Serial.print("✅ Split complete: ");
  Serial.print(nShares);
  Serial.print(" share files created (threshold = ");
  Serial.print(threshold);
  Serial.println(").");
  return sharePaths;
}

//=============================================================================
// reconstructFile:
//  - sharePaths: vector of at least 'threshold' file paths, each containing lines "x,y"
//  - threshold: the minimum number of shares needed; must match what was used in splitFile
//  - outputPath: path where the recovered file will be written (e.g., "/recovered.json")
// Returns true on success, false on failure.
//=============================================================================
bool ShamirSecretSharing::reconstructFile(const std::vector<String>& sharePaths, int threshold, const char* outputPath) {
  int availableShares = sharePaths.size();
  if (availableShares < threshold) {
    Serial.println("❌ Not enough shares provided to reconstruct.");
    return false;
  }

  // Open all provided share files in READ mode
  std::vector<File> shareFiles;
  shareFiles.reserve(availableShares);
  // xValues holds the "x" coordinate for each share (parsed from the first line of each file)
  std::vector<int> xValues(availableShares, 0);

  // Read first line of each share to extract its x (share index), then rewind file
  for (int idx = 0; idx < availableShares; idx++) {
    File sf = FFat.open(sharePaths[idx].c_str(), FILE_READ);
    if (!sf) {
      Serial.print("❌ Failed to open share file: ");
      Serial.println(sharePaths[idx]);
      // Close any already‐opened share files
      for (File& f : shareFiles) {
        f.close();
      }
      return false;
    }
    // Read the first line: "x,y"
    String firstLine = sf.readStringUntil('\n');
    sf.seek(0);  // rewind so we can read from the start when reconstructing each byte
    int commaIdx = firstLine.indexOf(',');
    if (commaIdx < 1) {
      Serial.print("❌ Invalid share file format (no comma): ");
      Serial.println(sharePaths[idx]);
      sf.close();
      for (File& f : shareFiles) {
        f.close();
      }
      return false;
    }

    // After reading firstLine = sf.readStringUntil('\n'):
int thisX = firstLine.substring(0, commaIdx).toInt();
Serial.print("  Share idx "); 
Serial.print(idx); 
Serial.print(" → x = "); 
Serial.println(thisX);

    // parse x coordinate
    xValues[idx] = firstLine.substring(0, commaIdx).toInt();
    shareFiles.push_back(sf);
  }

  // Determine number of bytes (lines) per share by counting lines in the first share
  int byteCount = 0;
  {
    File& firstShare = shareFiles[0];
    while (firstShare.available()) {
      firstShare.readStringUntil('\n');
      byteCount++;
    }
    firstShare.seek(0);  // rewind to start for reconstruction
  }

  // Prepare the output file to write the reconstructed bytes
  File outFile = FFat.open(outputPath, FILE_WRITE);
  if (!outFile) {
    Serial.print("❌ Failed to create output file: ");
    Serial.println(outputPath);
    for (File& sf : shareFiles) {
      sf.close();
    }
    return false;
  }

  // Buffers to hold y‐values for each share for a given byte index
  std::vector<int> yValues(threshold, 0);
  std::vector<int> xSubset(threshold, 0);

  // For each byte position (0..byteCount-1), read one line from each share and interpolate
  for (int byteIdx = 0; byteIdx < byteCount; byteIdx++) {
    // Read the next line from each share file, but only consider the first 'threshold' shares
    for (int s = 0; s < threshold; s++) {
      String line = shareFiles[s].readStringUntil('\n');
      int commaPos = line.indexOf(',');
      if (commaPos < 1) {
        Serial.println("❌ Malformed share line during reconstruction.");
        // Cleanup
        outFile.close();
        for (File& sf : shareFiles) {
          sf.close();
        }
        return false;
      }
      xSubset[s] = xValues[s];                          // share’s x coordinate
      yValues[s] = line.substring(commaPos + 1).toInt(); // share’s y coordinate
    }

    // Compute the original byte at x=0 via Lagrange interpolation
    int recovered = lagrangeInterpolation(0, xSubset, yValues);
    // Write the raw byte to the output file (cast to uint8_t)
    outFile.write((uint8_t)recovered);
    // Next iteration will process the next lines
  }

  // Close all files
  outFile.close();
  for (File& sf : shareFiles) {
    sf.close();
  }

  Serial.print("✅ Reconstruction complete: ");
  Serial.print(byteCount);
  Serial.print(" bytes written to ");
  Serial.println(outputPath);
  return true;
}

//=============================================================================
// evalPolynomial: f(x) = (secret + randomCoeff * x) mod PRIME
//=============================================================================
inline uint8_t ShamirSecretSharing::evalPolynomial(uint8_t x, uint8_t secret, uint8_t randomCoeff) {
  int val = ((int)secret + (int)randomCoeff * (int)x) % PRIME;
  return (uint8_t)val;
}

//=============================================================================
// modInverse: Extended Euclidean Algorithm to find modular inverse of 'a' modulo 'm'
// Returns inverse in [0..m-1], or 0 if no inverse (should not happen if m is prime and a != 0)
//=============================================================================
int ShamirSecretSharing::modInverse(int a, int m) {
  int m0 = m;
  int x0 = 0, x1 = 1;
  if (m == 1) return 0;
  while (a > 1) {
    int q = a / m;
    int t = m;
    m = a % m;    // m is remainder now
    a = t;
    t = x0;
    x0 = x1 - q * x0;
    x1 = t;
  }
  // Make x1 positive
  if (x1 < 0) x1 += m0;
  return x1;
}

//=============================================================================
// lagrangeInterpolation: Given vectors xVals[0..k-1], yVals[0..k-1], compute f(atX)
// over GF(PRIME), where f is a polynomial of degree < k that matches the given points.
// Typically atX = 0 to recover the secret.
//=============================================================================
int ShamirSecretSharing::lagrangeInterpolation(int atX, const std::vector<int>& xVals, const std::vector<int>& yVals) {
  int k = xVals.size();  // should equal yVals.size()
  long result = 0;

  // For each share i, compute its Lagrange basis polynomial Li(atX)
  for (int i = 0; i < k; i++) {
    long numerator = 1;
    long denominator = 1;
    for (int j = 0; j < k; j++) {
      if (j == i) continue;
      // Multiply numerator by (atX - xVals[j]) mod PRIME
      numerator = (numerator * (atX - xVals[j] + PRIME)) % PRIME;
      // Multiply denominator by (xVals[i] - xVals[j]) mod PRIME
      denominator = (denominator * (xVals[i] - xVals[j] + PRIME)) % PRIME;
    }
    // Compute modular inverse of denominator
    int invDenom = modInverse((int)(denominator % PRIME), PRIME);
    long term = ((long)yVals[i] * numerator % PRIME) * invDenom % PRIME;
    result = (result + term) % PRIME;
  }

  // result now holds f(atX) in [0..PRIME-1]
  return (int)result;
}
