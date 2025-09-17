#include <Arduino.h>
#include <FFat.h>
#include <ShamirHelper.h>


const char* TEST_PATH      = "/test.txt";
const char* RECOVERED_PATH = "/recovered.txt";

void createTestFile() {
  FFat.remove(TEST_PATH);
  File f = FFat.open(TEST_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("❌ Could not create /test.txt");
    return;
  }
  const char* content = "Hello, Shamir Secret Sharing!\nLine2: 12345\n";
  f.print(content);
  f.close();
  Serial.println("✅ /test.txt created.");
}

void printRecoveredHex(const char* path) {
  File f = FFat.open(path, FILE_READ);
  if (!f) {
    Serial.print("❌ Can't open ");
    Serial.println(path);
    return;
  }
  Serial.println("\n--- Hex Dump of recovered.txt ---");
  int idx = 0;
  while (f.available()) {
    uint8_t b = f.read();
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(' ');
    if (b >= 32 && b <= 126) {
      Serial.print('(');
      Serial.print((char)b);
      Serial.print(") ");
    } else {
      Serial.print("(.) ");
    }
    idx++;
    if ((idx % 16) == 0) Serial.println();
  }
  Serial.println("\n--- End Hex Dump ---");
  f.close();
}

void setup() {
  Serial.begin(115200);
  delay(10000);
  Serial.println("\n=== ShamirSS Sanity Check ===");

  // 1) Mount FFat (format if needed)
  if (!FFat.begin()) {
    Serial.println("❌ FFat mount failed; formatting...");
    FFat.format();
    if (!FFat.begin()) {
      Serial.println("❌ FFat still failed. Halting.");
      while (true);
    }
  }
  Serial.println("✅ FFat mounted.");
  const char* tst = R"JSON([
{"timestamp":"2025-06-07T19:05:36Z","isEvent":false,"data":{"lat":31.97087,"lon":34.78566,"heartRate":78,"name":"How Are"}}
,
{"timestamp":"2025-06-07T19:05:43Z","isEvent":false,"data":{"lat":31.97087,"lon":34.78568,"heartRate":100,"name":"Im Good"}}
,
{"timestamp":"2025-06-07T19:05:50Z","isEvent":false,"data":{"lat":31.97086,"lon":34.78564,"heartRate":55,"name":"How Are"}}
,
{"timestamp":"2025-06-07T19:05:57Z","isEvent":false,"data":{"lat":31.97084,"lon":34.78562,"heartRate":0,"name":"How Are"}}
])JSON";

  Serial.println(tst);
  // 2) Create a known test file
  //createTestFile();

  File f1 = FFat.open(TEST_PATH, FILE_WRITE);
  if (!f1) {
    Serial.println("❌ Could not create /test.txt");
    return;
  }
  f1.print(tst);
  f1.close();
  Serial.println("✅ /test.txt created.");

  // 3) Seed random BEFORE splitting
  randomSeed(micros());

  // 4) Split "/test.txt" into 3 shares, threshold = 2
  std::vector<String> sharePaths;

  std::vector<uint8_t> sharePaths1;
  sharePaths1.push_back(1);
  sharePaths1.push_back(3);
  sharePaths1.push_back(2);
  ShamirHelper::splitFile(TEST_PATH, 3, sharePaths1, sharePaths);
  if (sharePaths.size() < 3) {
    Serial.println("❌ splitFile() failed. Halting.");
    while (true);
  }

  // Print which two shares we’re about to use
  Serial.print("Reconstructing from: ");
  Serial.print(sharePaths[0]);  // should be "/test.txt.share1"
  Serial.print("  +  ");
  Serial.println(sharePaths[1]); // should be "/test.txt.share2"

  Serial.print("Reconstructing from shares: ");
  Serial.print(sharePaths[0]);   // should be "/test.txt.share1"
  Serial.print("  and  ");
  Serial.println(sharePaths[1]); // should be "/test.txt.share2"

  // 5) Reconstruct using share1 & share2
  bool ok = ShamirHelper::reconstructFile(sharePaths, RECOVERED_PATH);
  if (!ok) {
    Serial.println("❌ reconstructFile() failed.");
    while (true);
  }


  // 6) Print a hex‐dump of every byte in the recovered file, so you can see exactly which ASCII codes came back
  Serial.println("print hex using recovered path:");
  printRecoveredHex(RECOVERED_PATH);

  // 7) Also print the final ASCII text so you can compare
  File f = FFat.open(RECOVERED_PATH, FILE_READ);
  Serial.println("\n--- ASCII Dump of recovered.txt ---");
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println("\n--- End ASCII Dump ---");
  f.close();

  Serial.println("\n=== Sanity Check Complete ===");
}

void loop() {
  // nothing to do here
}
