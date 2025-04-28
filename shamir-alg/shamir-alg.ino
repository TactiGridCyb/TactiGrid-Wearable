#include <WiFi.h>
#include <HTTPClient.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <FFat.h>
#include <lvgl.h>

#define PRIME 257
#define SHARES 3
#define THRESHOLD 2

const char* ssid = "*******";
const char* password = "*******";
const char* serverUrl = "http://192.168.1.141:5000/upload";

lv_obj_t* send_btn;
lv_obj_t* upload_btn;
lv_obj_t* view_btn;
lv_obj_t* delete_btn;
lv_obj_t* reconstruct_btn;
lv_obj_t* print_btn;
String secret = "this is a secret file that contains the mission's log";

uint8_t evalPolynomial(uint8_t x, uint8_t secret, uint8_t randomCoeff) {
  return (secret + randomCoeff * x) % PRIME;
}

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

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

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

void uploadExistingFiles() {
  sendFile("/file1.txt");
  sendFile("/file2.txt");
  sendFile("/file3.txt");
  Serial.println("All files uploaded.");
}

void sendAllFiles() {
  Serial.println("--- Send Shares button pressed ---");
  splitSecret();
}

// Modular exponentiation: (base^exp) % mod
int modPow(int base, int exp, int mod) {
  int result = 1;
  base = base % mod;
  while (exp > 0) {
    if (exp % 2 == 1) result = (result * base) % mod;
    exp = exp >> 1;
    base = (base * base) % mod;
  }
  return result;
}


void reconstructSecret() {
  File files[THRESHOLD];
  int points[THRESHOLD][2];

  for (int i = 0; i < THRESHOLD; i++) {
    String filename = "/file" + String(i + 1) + ".txt";
    files[i] = FFat.open(filename);
    if (!files[i]) {
      Serial.printf("Failed to open %s\n", filename.c_str());
      return;
    }
  }

  File reconstructed = FFat.open("/reconstructed.txt", FILE_WRITE);
  if (!reconstructed) {
    Serial.println("Failed to create reconstructed file");
    return;
  }

  while (files[0].available()) {
    for (int i = 0; i < THRESHOLD; i++) {
      files[i].parseInt(); // x value (not used)
      points[i][0] = i + 1;
      points[i][1] = files[i].parseInt();
    }

    int numerator = 0;
    for (int i = 0; i < THRESHOLD; i++) {
      int term = points[i][1];
      for (int j = 0; j < THRESHOLD; j++) {
        if (i != j) {
          term = (term * (-points[j][0])) % PRIME;
          term = (term * modPow(points[i][0] - points[j][0], PRIME - 2, PRIME)) % PRIME;
        }
      }
      numerator = (numerator + term + PRIME) % PRIME;
    }

    reconstructed.write((char)numerator);
  }

  for (int i = 0; i < THRESHOLD; i++) files[i].close();
  reconstructed.close();

  Serial.println("File reconstructed successfully.");
}

void printReconstructed() {
  File f = FFat.open("/reconstructed.txt");
  if (!f) {
    Serial.println("Failed to open reconstructed.txt");
    return;
  }
  Serial.println("--- Content of reconstructed.txt ---");
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println("\n------------------------------------");
}

void on_send_btn_event(lv_event_t* e) {
  sendAllFiles();
}

void on_upload_btn_event(lv_event_t* e) {
  Serial.println("--- Upload Files button pressed ---");
  connectToWiFi();
  uploadExistingFiles();
}

void on_view_btn_event(lv_event_t* e) {
  Serial.println("--- View Files button pressed ---");
  listFileNames();
}

void on_delete_btn_event(lv_event_t* e) {
  Serial.println("--- Delete Shares button pressed ---");
  deleteShareFiles();
}

void on_reconstruct_btn_event(lv_event_t* e) {
  Serial.println("--- Reconstruct File button pressed ---");
  reconstructSecret();
}

void on_print_btn_event(lv_event_t* e) {
  Serial.println("--- Print Reconstructed File ---");
  printReconstructed();
}

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

  lv_obj_t* label;

  send_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(send_btn, 200, 50);
  lv_obj_align(send_btn, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_add_event_cb(send_btn, on_send_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(send_btn);
  lv_label_set_text(label, "Send Shares");
  lv_obj_center(label);

  upload_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(upload_btn, 200, 50);
  lv_obj_align(upload_btn, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_add_event_cb(upload_btn, on_upload_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(upload_btn);
  lv_label_set_text(label, "Upload Files");
  lv_obj_center(label);

  view_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(view_btn, 200, 50);
  lv_obj_align(view_btn, LV_ALIGN_TOP_MID, 0, 115);
  lv_obj_add_event_cb(view_btn, on_view_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(view_btn);
  lv_label_set_text(label, "View Files");
  lv_obj_center(label);

  delete_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(delete_btn, 200, 50);
  lv_obj_align(delete_btn, LV_ALIGN_TOP_MID, 0, 170);
  lv_obj_add_event_cb(delete_btn, on_delete_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(delete_btn);
  lv_label_set_text(label, "Delete Shares");
  lv_obj_center(label);

  reconstruct_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(reconstruct_btn, 200, 50);
  lv_obj_align(reconstruct_btn, LV_ALIGN_TOP_MID, 0, 225);
  lv_obj_add_event_cb(reconstruct_btn, on_reconstruct_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(reconstruct_btn);
  lv_label_set_text(label, "Reconstruct File");
  lv_obj_center(label);

  print_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(print_btn, 200, 50);
  lv_obj_align(print_btn, LV_ALIGN_TOP_MID, 0, 280);
  lv_obj_add_event_cb(print_btn, on_print_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(print_btn);
  lv_label_set_text(label, "Print File Content");
  lv_obj_center(label);
}

void loop() {
  lv_task_handler();
}
