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

const char* ssid = "Xiaomi 13T dgaming";
const char* password = "ctc5xwpwct";

SX1262 lora = newModule();
lv_obj_t *current_marker = NULL;

const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
const char* tileFilePath = "/tile.png";

using GPSCoordTuple = std::tuple<float, float, float, float>;

volatile bool pmu_flag = false;
lv_color_t ballColor = lv_color_hex(0xff0000);

void IRAM_ATTR onPmuInterrupt() {
    pmu_flag = true;
  }
  

void anim_opacity_cb(void * obj, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)obj, value, 0);
}

std::tuple<int, int, int> positionToTile(float lat, float lon, int zoom)
{
    
    float lat_rad = radians(lat);
    float n = pow(2.0, zoom);
    
    int x_tile = floor((lon + 180.0) / 360.0 * n);
    int y_tile = floor((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n);
    
    std::tuple<int, int, int> ans(zoom, x_tile, y_tile);

    return ans;
}

std::tuple<int,int> latlon_to_pixel(double lat, double lon, int zoom) {
    double lat_rad = lat * M_PI / 180.0;
    double n = pow(2.0, zoom);

    int pixel_x = (int)((lon + 180.0) / 360.0 * n * LV_HOR_RES_MAX) % LV_HOR_RES_MAX;
    int pixel_y = (int)((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / M_PI) / 2.0 * n * LV_HOR_RES_MAX) % LV_HOR_RES_MAX;

    return std::tuple<int,int>(pixel_x, pixel_y);
}

void create_fading_red_circle(double lat, double lon, int zoom) {
    if (current_marker != NULL) {
        lv_obj_del(current_marker);
        current_marker = NULL;
    }

    int pixel_x, pixel_y;
    std::tie(pixel_x, pixel_y) = latlon_to_pixel(lat, lon, zoom);

    current_marker = lv_obj_create(lv_scr_act());
    lv_obj_set_size(current_marker, 10, 10);
    lv_obj_set_style_bg_color(current_marker, ballColor, 0);
    lv_obj_set_style_radius(current_marker, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_pos(current_marker, pixel_x, pixel_y);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, current_marker);
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
        //Serial.println("Valid PNG file detected!");
        return true;
    } else {
        //Serial.println("Invalid file! Not a PNG.");
        return false;
    }
}

void checkTileSize(const char* tileFilePath) {
    if (FFat.exists(tileFilePath)) {
        File file = FFat.open(tileFilePath, FILE_READ);
        if (file) {
            size_t fileSize = file.size();
            //Serial.printf("Tile file size: %d bytes\n", fileSize);

            if (isPNG(file)) {
                //Serial.println("File is a valid PNG!");
            } else {
                //Serial.println("File is NOT a valid PNG.");
            }

            file.close();
        } else {
            //Serial.println("Failed to open tile file for size check!");
        }
    } else {
        //Serial.println("Tile file does not exist!");
    }
}

void listFiles(const char* dirname) {
    //Serial.printf("Listing files in: %s\n", dirname);

    File root = FFat.open(dirname);
    if (!root) {
        //Serial.println("Failed to open directory!");
        return;
    }
    if (!root.isDirectory()) {
        //Serial.println("Not a directory!");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            //Serial.printf("DIR : %s\n", file.name());
        } else {
            //Serial.printf("FILE: %s - %d bytes\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

void lv_example_img_1(void)
{
    
    //Serial.println("Creating LVGL image object...");

    lv_obj_t * img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);
    //Serial.println("Setting image source to S:/tile.png...");
    lv_img_set_src(img1, "A:/tile.png");

    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

}

void connectToWiFi() {
    
    Serial.print("Connecting to WiFi...");
    uint8_t x = -61;
    uint8_t* xp = &x;
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

bool saveTileToFFat(const uint8_t* data, size_t len, const char* tileFilePath) {
    File file = FFat.open(tileFilePath, FILE_WRITE);

    if (!file) {
        //Serial.println("Failed to open file for writing!");
        return false;
    }

    file.write(data, len);
    file.close();
    //Serial.println("Tile saved successfully to FFat!");
    return true;
}

bool downloadSingleTile(const std::string &tileURL, const char* fileName) {
    HTTPClient http;
    http.begin(tileURL.c_str());
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                 "AppleWebKit/537.36 (KHTML, like Gecko) "
                                 "Chrome/134.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.openstreetmap.org/");
    http.addHeader("Accept", "image/png");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        ballColor = lv_color_hex(0x000000);
        size_t fileSize = http.getSize();
        WiFiClient* stream = http.getStreamPtr();
        uint8_t* buffer = new uint8_t[fileSize];
        stream->readBytes(buffer, fileSize);

        bool success = saveTileToFFat(buffer, fileSize, fileName);
        delete[] buffer;
        http.end();
        return success;
    } else {
        http.end();
        return false;
    }
}

bool downloadTileIfNeeded(std::string tileURL, const char* fileName)
{
    if(!FFat.exists(fileName))
    {
        return downloadSingleTile(tileURL, fileName);
    }

    return true;
}

bool downloadTiles(const std::tuple<int, int, int> &tileLocation) {

    std::string middleTile = tileUrlInitial + std::to_string(std::get<0>(tileLocation))
    + "/" + std::to_string(std::get<1>(tileLocation)) +
    "/" + std::to_string(std::get<2>(tileLocation)) + ".png";

    std::string leftTile = tileUrlInitial + std::to_string(std::get<0>(tileLocation))
    + "/" + std::to_string(std::get<1>(tileLocation) - 1) + "/" +
    std::to_string(std::get<2>(tileLocation)) + ".png";

    std::string rightTile = tileUrlInitial + std::to_string(std::get<0>(tileLocation))
    + "/" + std::to_string(std::get<1>(tileLocation) + 1) + "/" +
    std::to_string(std::get<2>(tileLocation)) + ".png";

    std::string bottomTile = tileUrlInitial + std::to_string(std::get<0>(tileLocation))
    + "/" + std::to_string(std::get<1>(tileLocation)) + "/" +
    std::to_string(std::get<2>(tileLocation) + 1) + ".png";

    std::string topTile = tileUrlInitial + std::to_string(std::get<0>(tileLocation))
    + "/" + std::to_string(std::get<1>(tileLocation)) + "/" +
    std::to_string(std::get<2>(tileLocation) - 1) + ".png";

    //Serial.println(middleTile.c_str());
    //Serial.println(leftTile.c_str());
    //Serial.println(rightTile.c_str());
    //Serial.println(bottomTile.c_str());
    //Serial.println(topTile.c_str());

    bool downloadMiddleTile = downloadTileIfNeeded(middleTile, "/middleTile.png");
    bool downloadLeftTile = downloadTileIfNeeded(leftTile, "/leftTile.png");
    bool downloadRightTile = downloadTileIfNeeded(rightTile, "/rightTile.png");
    bool downloadBottomTile = downloadTileIfNeeded(bottomTile, "/bottomTile.png");
    bool downloadTopTile = downloadTileIfNeeded(topTile, "/topTile.png");

    return downloadMiddleTile || downloadLeftTile || downloadRightTile || downloadBottomTile || downloadTopTile;
}

void showTiles(const char* middleTilePath, const char* leftTilePath, const char* rightTilePath, const char* bottomTilePath, const char* topTilePath) {
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_t * centerTile = lv_img_create(mainPage);
    std::string TileSRC = std::string("A:") + middleTilePath + std::string(".png");
    lv_img_set_src(centerTile, TileSRC.c_str());
    lv_obj_set_size(centerTile, 120, 120);
    lv_obj_align(centerTile, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * leftTile = lv_img_create(mainPage);
    TileSRC = std::string("A:") + leftTilePath + std::string(".png");
    lv_img_set_src(leftTile, TileSRC.c_str());
    lv_obj_set_size(leftTile, 120, 120);
    lv_obj_align(leftTile, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * rightTile = lv_img_create(mainPage);
    TileSRC = std::string("A:") + rightTilePath + std::string(".png");
    lv_img_set_src(rightTile, TileSRC.c_str());
    lv_obj_set_size(rightTile, 120, 120);
    lv_obj_align(rightTile, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t * bottomTile = lv_img_create(mainPage);
    TileSRC = std::string("A:") + bottomTilePath + std::string(".png");
    lv_img_set_src(bottomTile, TileSRC.c_str());
    lv_obj_set_size(bottomTile, 120, 120);
    lv_obj_align(bottomTile, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    lv_obj_t * topTile = lv_img_create(mainPage);
    TileSRC = std::string("A:") + topTilePath + std::string(".png");
    lv_img_set_src(topTile, TileSRC.c_str());
    lv_obj_set_size(topTile, 120, 120);
    lv_obj_align(topTile, LV_ALIGN_TOP_MID, 0, 0);
    
    //Serial.println("Tile displayed on the screen!");
}

void deleteExistingTile(const char* tileFilePath) {
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

// === Initialize LoRa ===
void initializeLoRa() {
    Serial.print(F("[LoRa] Initializing... "));
    int state = lora.begin();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true);
    }
  
    lora.setFrequency(868.0);
    lora.setOutputPower(10);
    lora.setSpreadingFactor(9);
    lora.setBandwidth(125.0);
    lora.setCodingRate(5);
  }
  

void deleteExistingTiles(std::string middleTile, std::string topTile, std::string bottomTile, std::string leftTile, std::string rightTile) {
    
    deleteExistingTile(middleTile.c_str());
    deleteExistingTile(topTile.c_str());
    deleteExistingTile(bottomTile.c_str());
    deleteExistingTile(leftTile.c_str());
    deleteExistingTile(rightTile.c_str());

}

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);
}

void init_main_poc_page()
{
    static std::tuple<int,int,int> soldiersPosition = positionToTile(31.967336875793308, 34.78519519036414, 19);
    
    //Serial.print("Address of soldiersPosition: 0x");
    //Serial.println((uintptr_t)&soldiersPosition, HEX);

    clearMainPage();

    ballColor = lv_color_hex(0xff0000);

    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid POC");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);


    lv_obj_t *p2pTestBtn = lv_btn_create(mainPage);
    lv_obj_center(p2pTestBtn);
    lv_obj_set_style_bg_color(p2pTestBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(p2pTestBtn, init_p2p_test, LV_EVENT_CLICKED, (void*)&soldiersPosition);


    lv_obj_t *P2PTestLabel = lv_label_create(p2pTestBtn);
    lv_label_set_text(P2PTestLabel, "P2P Test");
    lv_obj_center(P2PTestLabel);

}


void setup() {
    Serial.begin(115200);
    watch.begin(&Serial);
    Serial.println("HELLO");
    
    if (!FFat.begin(true)) {
        Serial.println("Failed to mount FFat!");
        return;
    }
    Serial.println("WORLD");
    //listFiles("/");
    deleteExistingTile("/middleTile.png");
    deleteExistingTile("/leftTile.png");
    deleteExistingTile("/rightTile.png");
    deleteExistingTile("/bottomTile.png");
    deleteExistingTile("/topTile.png");


    listFiles("/");
    beginLvglHelper();
    lv_png_init();

    Serial.println("Set LVGL!");

    connectToWiFi();
    initializeLoRa();
    //init_main_poc_page();
    

    //Serial.println("init_main_poc_page");
    
    watch.attachPMU(onPmuInterrupt);

    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();
    watch.enableIRQ(
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
        XPOWERS_AXP2101_PKEY_LONG_IRQ
    );
}



void init_p2p_test(lv_event_t * event)
{

    //Serial.println("init_p2p_test");
    std::tuple<int, int, int>* soldiersPosition = static_cast<std::tuple<int, int, int>*>(lv_event_get_user_data(event));

    bool tileExists = FFat.exists(tileFilePath);
    clearMainPage();

    //deleteExistingTiles();

    if (downloadTiles(*soldiersPosition)) {
        //Serial.println(tileExists + " Tile download completed!");
        listFiles("/");
        //showTiles("/middleTile", "/leftTile", "/rightTile", "/bottomTile", "/topTile");
    } else {
        //Serial.println("Failed to download the tile.");
    }

    //create_fading_red_circle();
}

// === Parse into std::tuple ===
GPSCoordTuple parseCoordinates(const String &message) {
    float lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    sscanf(message.c_str(), "%f,%f;%f,%f", &lat1, &lon1, &lat2, &lon2);
    return std::make_tuple(lat1, lon1, lat2, lon2);
}

void loop() {
    if (pmu_flag) {
      pmu_flag = false;
      uint32_t status = watch.readPMU();
      if (watch.isPekeyShortPressIrq()) {
        // Reinitialize the main page on power key press
        init_main_poc_page();
      }
      watch.clearPMU();
    }
  
    String incoming;
    if (receiveMessage(incoming)) {

        Serial.println("Received: " + incoming);
        // Expecting a message like: "lat_tile,lon_tile;lat_marker,lon_marker"
        GPSCoordTuple coords = parseCoordinates(incoming);
        
        // Extract the coordinates:
        float tile_lat   = std::get<0>(coords);
        float tile_lon   = std::get<1>(coords);
        float marker_lat = std::get<2>(coords);
        float marker_lon = std::get<3>(coords);
    
        // Download the tile only once (use the first coordinate as the center tile)
        if (!FFat.exists("/middleTile")) {
            std::tuple<int, int, int> tileLocation = positionToTile(tile_lat, tile_lon, 19);
            if (downloadTiles(tileLocation)) {
                // Optionally, display the tile here (e.g., call your lv_example_img_1() or showTiles() function)
                // lv_example_img_1();  // or showTiles(...);
            }
            
        }
        
        // Create (or update) the marker on the map for the new marker coordinate
        create_fading_red_circle(marker_lat, marker_lon, 19);
    }
  
    lv_task_handler();
    delay(10);
}

// === Receive LoRa message ===
bool receiveMessage(String &message) {

    //Serial.println(F("[LoRa] Listening for incoming message..."));
    int state = lora.receive(message);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[LoRa] Message received!"));
        Serial.print(F("[LoRa] Data: "));
        Serial.println(message);
        return true;
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        //Serial.println(F("[LoRa] Timeout – no message received."));
    } else {
        Serial.print(F("[LoRa] Receive failed, code "));
        Serial.println(state);
    }
    return false;
}

//TODO : Add state of current page to avoid power button presses when already on home page