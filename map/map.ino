#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <FFat.h>
#include <lvgl.h>
#include <LV_Helper.h>
#include <LilyGoLib.h>

// WiFi credentials
const char* ssid = "default";
const char* password = "1357924680";

// OpenStreetMap tile URL
const char* tileUrl = "https://tile.openstreetmap.org/8/152/103.png";

// Tile file name to save in FFat
const char* tileFile = "/tile.png";

// TFT and LVGL objects
lv_disp_draw_buf_t draw_buf;
lv_color_t buf[LV_HOR_RES_MAX * 10];

bool isPNG(File file) {
    uint8_t header[8];
    file.read(header, sizeof(header));

    // PNG signature: 89 50 4E 47 0D 0A 1A 0A
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E &&
        header[3] == 0x47 && header[4] == 0x0D && header[5] == 0x0A &&
        header[6] == 0x1A && header[7] == 0x0A) {
        Serial.println("Valid PNG file detected!");
        return true;
    } else {
        Serial.println("Invalid file! Not a PNG.");
        return false;
    }
}

void checkTileSize() {
    if (FFat.exists(tileFile)) {
        File file = FFat.open(tileFile, FILE_READ);
        if (file) {
            size_t fileSize = file.size();
            Serial.printf("Tile file size: %d bytes\n", fileSize);

            // Check if the file is a valid PNG
            if (isPNG(file)) {
                Serial.println("File is a valid PNG!");
            } else {
                Serial.println("File is NOT a valid PNG.");
            }

            file.close();
        } else {
            Serial.println("Failed to open tile file for size check!");
        }
    } else {
        Serial.println("Tile file does not exist!");
    }
}

void listFiles(const char* dirname) {
    Serial.printf("Listing files in: %s\n", dirname);

    File root = FFat.open(dirname);  // Use FFat, SPIFFS, or LittleFS depending on your FS
    if (!root) {
        Serial.println("Failed to open directory!");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory!");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("DIR : %s\n", file.name());
        } else {
            Serial.printf("FILE: %s - %d bytes\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

void lv_example_img_1(void)
{
    
    Serial.println("Creating LVGL image object...");
    String file =  tileFile;


    lv_obj_t * img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);
    Serial.println("Setting image source to S:/tile.png...");
    lv_img_set_src(img1, "A:/tile.png");
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    // Optional: scale up the image for better visibility if it's small
    // lv_img_set_zoom(img1, 4096); // 200% zoom

    // Add a red border to see image boundaries
    lv_obj_set_style_border_color(img1, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_border_width(img1, 2, 0);

    Serial.println("Screen invalidated for redraw.");

}

// Connect to WiFi
void connectToWiFi() {
    
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

// Save the tile to FFat
bool saveTileToFFat(const uint8_t* data, size_t len) {
    File file = FFat.open(tileFile, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing!");
        return false;
    }
    file.write(data, len);
    file.close();
    Serial.println("Tile saved successfully to FFat!");
    return true;
}

// Download tile using HTTPClient
bool downloadTile() {
    HTTPClient http;

    // Configure HTTP headers
    http.begin(tileUrl);
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                 "AppleWebKit/537.36 (KHTML, like Gecko) "
                                 "Chrome/134.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.openstreetmap.org/");
    http.addHeader("Accept", "image/png");

    // Send HTTP GET request
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        // Get file data
        size_t fileSize = http.getSize();
        WiFiClient* stream = http.getStreamPtr();

        uint8_t* buffer = new uint8_t[fileSize];
        stream->readBytes(buffer, fileSize);

        // Save to FFat
        if (saveTileToFFat(buffer, fileSize)) {
            Serial.println("Tile successfully downloaded and saved to FFat!");
        } else {
            Serial.println("Failed to save tile to FFat.");
        }
        delete[] buffer;
    } else {
        Serial.printf("HTTP request failed! Error: %s\n", http.errorToString(httpCode).c_str());
        return false;
    }

    http.end();
    return true;
}

// Show downloaded tile on screen
void showTile() {
    File file = FFat.open(tileFile, FILE_READ);
    if (!file) {
        Serial.println("Failed to open tile file from FFat!");
        return;
    }

    file.close();

    lv_example_img_1();
    
    Serial.println("Tile displayed on the screen!");
}



void deleteExistingTile() {
    if (FFat.exists(tileFile)) {
        Serial.println("Pre Delete!");
        if (FFat.remove(tileFile)) {
            Serial.println("Existing tile deleted successfully!");
        } else {
            Serial.println("Failed to delete existing tile.");
        }
    } else {
        Serial.println("No existing tile found.");
    }
}

void setup() {
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();
    lv_png_init();

    // Initialize FFat
    if (!FFat.begin(true)) {
        Serial.println("Failed to mount FFat!");
        return;
    }

    Serial.println("Set PNG decoder!");
    deleteExistingTile();

    // Connect to WiFi
    connectToWiFi();

    // Download the tile and save it
    if (downloadTile()) {
        Serial.println("Tile download completed!");
        listFiles("/");
        checkTileSize();
        showTile();  // Show tile after download
    } else {
        Serial.println("Failed to download the tile.");
    }


}

void loop() {
    lv_timer_handler();  // Handle LVGL tasks
    delay(5);
}
