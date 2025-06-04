//************************just the creation of the original and shared files + saving them on the watch */
// #include "LilyGoLib.h"
// #include "FFat.h"

// #define PRIME 257
// #define SHARES 3
// #define THRESHOLD 2

// // The original secret string to split
// String secret = "this is a secret file containing mission's log";

// // Evaluate the polynomial: f(x) = secret + randomCoeff * x mod PRIME
// uint8_t evalPolynomial(uint8_t x, uint8_t secret, uint8_t randomCoeff) {
//   return (secret + randomCoeff * x) % PRIME;
// }

// // Split the secret string into SHARES parts using Shamir's Secret Sharing
// void splitSecret(String secret) {
//   File files[SHARES];
//   for (int i = 0; i < SHARES; i++) {
//     String filename = "/file" + String(i + 1) + ".txt";
//     files[i] = FFat.open(filename.c_str(), FILE_WRITE);
//   }

//   for (int i = 0; i < secret.length(); i++) {
//     uint8_t s = (uint8_t)secret[i];                   // secret byte
//     uint8_t r = random(1, PRIME);                     // random coefficient
//     for (int x = 1; x <= SHARES; x++) {
//       uint8_t y = evalPolynomial(x, s, r);            // share value
//       files[x - 1].printf("%d,%d\n", x, y);           // save (x,y)
//     }
//   }

//   for (int i = 0; i < SHARES; i++) files[i].close();
// }

// // Write the original secret message to a file (/original.txt)
// void writeOriginal() {
//   File file = FFat.open("/original.txt", FILE_WRITE);
//   if (file) {
//     file.println(secret);
//     file.close();
//     Serial.println("Original file written.");
//   } else {
//     Serial.println("Failed to write original file.");
//   }
// }

// // Print the content of a file to the Serial Monitor
// void printFile(const char* path) {
//   File f = FFat.open(path);
//   if (!f) {
//     Serial.printf("Failed to open %s\n", path);
//     return;
//   }
//   Serial.printf("Content of %s:\n", path);
//   while (f.available()) {
//     Serial.write(f.read());
//   }
//   f.close();
//   Serial.println();
// }

// // List all files in the FFat root directory
// void listAllFiles() {
//   Serial.println("Listing all FFat files:");
//   File root = FFat.open("/");
//   if (!root) {
//     Serial.println("Failed to open root directory.");
//     return;
//   }
//   File entry = root.openNextFile();
//   while (entry) {
//     Serial.println(entry.name());
//     entry = root.openNextFile();
//   }
// }

// // Main setup function: initializes watch, mounts FFat, writes original and splits
// void setup() {
//   Serial.begin(115200);
//   watch.begin(&Serial);
//   delay(2000);

//   if (!FFat.begin()) {
//     Serial.println("FFat Mount Failed. Formatting...");
//     FFat.format();
//     FFat.begin();
//   }

//   writeOriginal();
//   splitSecret(secret);
//   Serial.println("Secret split into 3 share files.");

//   // Step 2: Print file contents to verify
//   printFile("/original.txt");
//   printFile("/file1.txt");
//   printFile("/file2.txt");
//   printFile("/file3.txt");

//   // Step 3: List all files
//   listAllFiles();
// }

// // Loop function (not used here)
// void loop() {
//   // Nothing here for now
// }


// ********* adding wifi and uploding to a site
// right now the division is for 3 shares, with minimum of 2 for reconstruction 
#include <WiFi.h>
#include <HTTPClient.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <FFat.h>
#include <lvgl.h>

#define PRIME 257
#define SHARES 3
#define THRESHOLD 2

// connect to wifi - in order to then upload the files to the webapp
const char* ssid = "peretz1";             // Wi-Fi name 
const char* password = "box17box";     // Wi-Fi password 
const char* serverUrl = "http://192.168.1.141:5000/upload";  // server IP

lv_obj_t* send_btn;                               // GUI button object
lv_obj_t* upload_btn;                             // Second button object
lv_obj_t* view_btn;                               // Button to view saved files
lv_obj_t* delete_btn;                             // Button to delete share files
String secret = "this is a secret file that contains the mission's log";

// Evaluate a polynomial at a given x, for secret sharing
uint8_t evalPolynomial(uint8_t x, uint8_t secret, uint8_t randomCoeff) {
  return (secret + randomCoeff * x) % PRIME;
}

// Split the original secret into share files and save to FFat
void splitSecret() {
  File files[SHARES];
  for (int i = 0; i < SHARES; i++) {
    String filename = "/file" + String(i + 1) + ".txt";
    files[i] = FFat.open(filename.c_str(), FILE_WRITE);
    if (!files[i]) {
      Serial.printf("Failed to open %s for writing\n", filename.c_str());
      return;
    }
  }

  for (int i = 0; i < secret.length(); i++) {
    uint8_t s = (uint8_t)secret[i];
    uint8_t r = random(1, PRIME);
    for (int x = 1; x <= SHARES; x++) {
      uint8_t y = evalPolynomial(x, s, r);
      files[x - 1].printf("%d,%d\n", x, y);
    }
  }

  for (int i = 0; i < SHARES; i++) files[i].close();
  Serial.println("Secret split and saved to FFat.");
}

// List all file names stored on FFat
void listFileNames() {
  File root = FFat.open("/");
  if (!root) {
    Serial.println("Failed to open root directory.");
    return;
  }

  File entry = root.openNextFile();
  Serial.println("Files on FFat:");
  while (entry) {
    Serial.println(entry.name());
    entry = root.openNextFile();
  }
  root.close();
}

// Delete share files from FFat
void deleteShareFiles() {
  for (int i = 1; i <= SHARES; i++) {
    String filename = "/file" + String(i) + ".txt";
    if (FFat.remove(filename)) {
      Serial.printf("Deleted %s\n", filename.c_str());
    } else {
      Serial.printf("Failed to delete %s or file not found\n", filename.c_str());
    }
  }
}

// Connect the watch to Wi-Fi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

// Send one share file to the Flask server using HTTP POST (safe body-based version)
void sendFile(const char* filepath) {
  Serial.printf("Preparing to upload: %s\n", filepath);
  if (WiFi.status() == WL_CONNECTED) {
    File file = FFat.open(filepath, "r");
    if (!file) {
      Serial.printf("Failed to open %s\n", filepath);
      return;
    }

    String start = "--123456789\r\nContent-Disposition: form-data; name=\"files[]\"; filename=\"" + String(filepath).substring(1) + "\"\r\nContent-Type: text/plain\r\n\r\n";
    String end = "\r\n--123456789--\r\n";

    String body = start;
    while (file.available()) {
      body += (char)file.read();
    }
    body += end;
    file.close();

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "multipart/form-data; boundary=123456789");

    int code = http.sendRequest("POST", body);
    Serial.printf("Upload %s: HTTP status %d\n", filepath, code);

    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}

// Upload existing files to the server
void uploadExistingFiles() {
  sendFile("/file1.txt");
  sendFile("/file2.txt");
  sendFile("/file3.txt");
  Serial.println("All files uploaded.");
}

// Split only, do not upload
void sendAllFiles() {
  Serial.println("--- Send Shares button pressed ---");
  splitSecret();
}

// Handle "Send Shares" button press
void on_send_btn_event(lv_event_t* e) {
  sendAllFiles();
}

// Handle "Upload Files" button press
void on_upload_btn_event(lv_event_t* e) {
  Serial.println("--- Upload Files button pressed ---");
  connectToWiFi();
  uploadExistingFiles();
}

// Handle "View Files" button press
void on_view_btn_event(lv_event_t* e) {
  Serial.println("--- View Files button pressed ---");
  listFileNames();
}

// Handle "Delete Files" button press
void on_delete_btn_event(lv_event_t* e) {
  Serial.println("--- Delete Shares button pressed ---");
  deleteShareFiles();
}

// Setup the watch, LVGL button UI, and FFat filesystem
void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  beginLvglHelper();
  delay(2000);

  if (!FFat.begin()) {
    Serial.println("Mounting FFat failed. Trying format...");
    if (FFat.format()) {
      Serial.println("Format successful.");
      if (!FFat.begin()) {
        Serial.println("Still failed after format. Aborting.");
        return;
      }
    } else {
      Serial.println("Format failed.");
      return;
    }
  }

  // Create "Send Shares" button
  lv_obj_t* label;
  send_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(send_btn, 200, 60);
  lv_obj_align(send_btn, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_add_event_cb(send_btn, on_send_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(send_btn);
  lv_label_set_text(label, "Send Shares");
  lv_obj_center(label);

  // Create "Upload Files" button
  upload_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(upload_btn, 200, 60);
  lv_obj_align(upload_btn, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(upload_btn, on_upload_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(upload_btn);
  lv_label_set_text(label, "Upload Files");
  lv_obj_center(label);

  // Create "View Files" button
  view_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(view_btn, 200, 60);
  lv_obj_align(view_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(view_btn, on_view_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(view_btn);
  lv_label_set_text(label, "View Files");
  lv_obj_center(label);

  // Create "Delete Files" button
  delete_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(delete_btn, 200, 60);
  lv_obj_align(delete_btn, LV_ALIGN_BOTTOM_MID, 0, 70);
  lv_obj_add_event_cb(delete_btn, on_delete_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(delete_btn);
  lv_label_set_text(label, "Delete Shares");
  lv_obj_center(label);
}

// Required loop for LVGL GUI
void loop() {
  lv_task_handler();
}
