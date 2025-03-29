#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

#include <tuple>
#include <FFat.h>
#include <FS.h>

#include <WiFi.h>
#include <HTTPClient.h>

#include <lvgl.h>
#include <LV_Helper.h>

#include <LilyGoLib.h>

const char* ssid = "XXX";
const char* password = "XXX";

const char* tileUrlInitial = "https://tile.openstreetmap.org/19/312787/212958.png";

const char* tileFilePath = "/tile.png";


void anim_opacity_cb(void * obj, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)obj, value, 0);
}

std::tuple<float, float> positionToTile(float lat, float lon, int zoom)
{
    std::tuple<float, float> ans;
    

    return ans;
}

void create_fading_red_circle() {
    lv_obj_t * circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(circle, 40, 40);
    lv_obj_center(circle);           

    lv_obj_set_style_bg_color(circle, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, circle);

    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_opacity_cb);

    lv_anim_start(&a);
}

bool isPNG(File file) {
    uint8_t header[8];
    file.read(header, sizeof(header));

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
    if (FFat.exists(tileFilePath)) {
        File file = FFat.open(tileFilePath, FILE_READ);
        if (file) {
            size_t fileSize = file.size();
            Serial.printf("Tile file size: %d bytes\n", fileSize);

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

    File root = FFat.open(dirname);
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

    lv_obj_t * img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);
    Serial.println("Setting image source to S:/tile.png...");
    lv_img_set_src(img1, "A:/tile.png");

    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

}

void connectToWiFi() {
    
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

bool saveTileToFFat(const uint8_t* data, size_t len) {
    File file = FFat.open(tileFilePath, FILE_WRITE);

    if (!file) {
        Serial.println("Failed to open file for writing!");
        return false;
    }

    file.write(data, len);
    file.close();
    Serial.println("Tile saved successfully to FFat!");
    return true;
}

bool downloadTile() {
    HTTPClient http;

    http.begin(tileUrlInitial);
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                 "AppleWebKit/537.36 (KHTML, like Gecko) "
                                 "Chrome/134.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.openstreetmap.org/");
    http.addHeader("Accept", "image/png");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {

        size_t fileSize = http.getSize();
        WiFiClient* stream = http.getStreamPtr();

        uint8_t* buffer = new uint8_t[fileSize];
        stream->readBytes(buffer, fileSize);

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

void showTile() {
    File file = FFat.open(tileFilePath, FILE_READ);
    if (!file) {
        Serial.println("Failed to open tile file from FFat!");
        return;
    }

    file.close();

    lv_example_img_1();
    
    Serial.println("Tile displayed on the screen!");
}



void deleteExistingTile() {
    if (FFat.exists(tileFilePath)) {
        Serial.println("Pre Delete!");
        if (FFat.remove(tileFilePath)) {
            Serial.println("Existing tile deleted successfully!");
        } else {
            Serial.println("Failed to delete existing tile.");
        }
    } else {
        Serial.println("No existing tile found.");
    }
}



void init_main_poc_page()
{
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid POC");
    lv_obj_center(mainLabel);

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_t *p2pTestBtn = lv_btn_create(mainPage);
    lv_obj_center(p2pTestBtn);
    lv_obj_set_style_bg_color(p2pTestBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(p2pTestBtn, init_p2p_test, LV_EVENT_CLICKED, NULL);


    lv_obj_t *P2PTestLabel = lv_label_create(p2pTestBtn);
    lv_label_set_text(P2PTestLabel, "P2P Test");
    lv_obj_center(P2PTestLabel);

}

void setup() {
    Serial.begin(115200);
    watch.begin(&Serial);

    if (!FFat.begin(true)) {
        Serial.println("Failed to mount FFat!");
        return;
    }

    beginLvglHelper();
    lv_png_init();

    Serial.println("Set LVGL!");

    deleteExistingTile();

    init_main_poc_page();
    Serial.println("init_main_poc_page");
}

void init_p2p_test(lv_event_t * event)
{
    Serial.println("init_p2p_test");

    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    connectToWiFi();
    
    if (downloadTile()) {
        Serial.println("Tile download completed!");
        //listFiles("/");
        checkTileSize();
        showTile();
    } else {
        Serial.println("Failed to download the tile.");
    }

    create_fading_red_circle();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
