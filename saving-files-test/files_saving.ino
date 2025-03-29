#include "LilyGoLib.h"
#include "FFat.h" 

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  
  Serial.println("LilyGo Watch-S3 initialized.");

  Serial.println("Writing to file /example.txt...");

  File file = FFat.open("/example.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing!");
  } else {
    file.println("This is a test file.");
    file.println("Storing data on FFat filesystem using LilyGoLib!");
    file.close();
    Serial.println("File written successfully.");
  }

  Serial.println("Reading from file /example.txt...");
  file = FFat.open("/example.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading!");
  } else {
    Serial.println("File Content:");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();
    Serial.println("\nFile read complete.");
  }

  Serial.println("Listing files in the root directory:");
  File root = FFat.open("/");
  if (!root) {
    Serial.println("Failed to open root directory!");
  } else {
    File entry = root.openNextFile();
    while (entry) {
      Serial.print("File: ");
      Serial.println(entry.name());
      entry.close();
      entry = root.openNextFile();
    }
    root.close();
  }
}

void loop() {
  // You can place other code here if necessary (but for this example, it's just for demonstration).
  // There's no need to repeatedly save/load in the loop since we already do that in setup().
}
