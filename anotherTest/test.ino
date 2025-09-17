#include <LilyGoLib.h>
#include <sodium.h>
#include <LV_Helper.h>
#include "../shamir/ShamirHelper.h"

void createTestFile(const char* path, const char* content) {
  File f = FFat.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("❌ Failed to create test file.");
    return;
  }
  f.print(content);
  f.close();
}

void printFile(const char* path) {
  File f = FFat.open(path, FILE_READ);
  if (!f) {
    Serial.print("❌ Could not open file: ");
    Serial.println(path);
    return;
  }
  Serial.print("📄 Contents of ");
  Serial.print(path);
  Serial.println(":");
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println("\n---");
}

void testShamirSplitAndReconstruct() {
  const char* originalPath = "/test.txt";
  const char* recoveredPath = "/test.recovered.txt";
  const char* testContent = "This is a test message using Shamir Secret Sharing!";
  const int prime = 251;
  const int nShares = 5;
  const int threshold = 3;

  createTestFile(originalPath, testContent);

  std::vector<String> sharePaths;
  bool splitOk = ShamirHelper::splitFile(originalPath, nShares, threshold, prime, sharePaths);
  if (!splitOk) {
    Serial.println("❌ Split failed.");
    return;
  }

  std::vector<String> partialShares(sharePaths.begin(), sharePaths.begin() + threshold);
  bool recOk = ShamirHelper::reconstructFile(partialShares, threshold, recoveredPath, prime);
  if (!recOk) {
    Serial.println("❌ Reconstruction failed.");
    return;
  }

  printFile(originalPath);
  printFile(recoveredPath);
}

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  delay(500); 
  Serial.println("📟 Starting...");
  beginLvglHelper();

  if (!FFat.begin(true)) {
    Serial.println("❌ FFat mount failed.");
    return;
  }

  testShamirSplitAndReconstruct();
}

void loop() {
  delay(5);
  lv_task_handler();
}
