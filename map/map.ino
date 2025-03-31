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

WiFiUDP udp;

const char* ssid = "default";
const char* password = "1357924680";

SX1262 lora = newModule();
lv_obj_t *current_marker = NULL;

const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
const char* tileFilePath = "/middleTile.png";
const char* logFilePath = "/log.txt";

using GPSCoordTuple = std::tuple<float, float, float, float>;

volatile bool pmu_flag = false;
volatile bool receivedFlag = false;

lv_obj_t* soldiersNameLabel = NULL;

ICACHE_RAM_ATTR void setFlag(void)
{
    // we got a packet, set the flag
    receivedFlag = true;
}

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
    
    return std::make_tuple(zoom, x_tile, y_tile);
}

std::tuple<int,int> latlon_to_pixel(double lat, double lon, double centerLat, double centerLon, int zoom)
{
    static constexpr double TILE_SIZE = 256.0;

    auto toGlobal = [&](double la, double lo) {
        double rad = la * M_PI / 180.0;
        double n   = std::pow(2.0, zoom);
        int xg = (int) std::floor((lo + 180.0) / 360.0 * n * TILE_SIZE);
        int yg = (int) std::floor((1.0 - std::log(std::tan(rad) + 1.0 / std::cos(rad)) / M_PI) / 2.0 * n * TILE_SIZE);
        return std::make_pair(xg, yg);
    };

    auto [markerX, markerY] = toGlobal(lat, lon);
    auto [centerX, centerY] = toGlobal(centerLat, centerLon);

    int localX = markerX - centerX + (LV_HOR_RES_MAX / 2);
    int localY = markerY - centerY + (LV_VER_RES_MAX / 2);

    return std::make_tuple(localX, localY);
}

std::pair<double,double> tileCenterLatLon(int zoom, int x_tile, int y_tile)
{
    static constexpr double TILE_SIZE = 256.0;
    // Compute the center pixel of the tile
    int centerX = x_tile * TILE_SIZE + TILE_SIZE / 2;
    int centerY = y_tile * TILE_SIZE + TILE_SIZE / 2;
    
    double n = std::pow(2.0, zoom);
    double lon_deg = (double)centerX / (n * TILE_SIZE) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(M_PI * (1 - 2.0 * centerY / (n * TILE_SIZE))));
    double lat_deg = lat_rad * 180.0 / M_PI;
    
    return {lat_deg, lon_deg};
}

void create_fading_red_circle(double markerLat, double markerLon, double centerLat, double centerLon, int zoom) {
    if (current_marker != NULL) {
        lv_obj_del(current_marker);
        current_marker = NULL;
    }

    int pixel_x, pixel_y;
    std::tie(pixel_x, pixel_y) = latlon_to_pixel(markerLat, markerLon, centerLat, centerLon, zoom);

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

    if(!soldiersNameLabel)
    {
        soldiersNameLabel = lv_label_create(lv_scr_act());
        lv_label_set_text(soldiersNameLabel, "Soldier 1");
    }
    
    
    lv_obj_align_to(soldiersNameLabel, current_marker, LV_ALIGN_TOP_MID, 0, -35);
}

bool saveTileToFFat(const uint8_t* data, size_t len, const char* tileFilePath) {
    File file = FFat.open(tileFilePath, FILE_WRITE);
    if (!file) {
        return false;
    }
    file.write(data, len);
    file.close();
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

bool downloadMiddleTile(const std::tuple<int, int, int> &tileLocation)
{
    std::string middleTile = tileUrlInitial +
        std::to_string(std::get<0>(tileLocation)) + "/" +
        std::to_string(std::get<1>(tileLocation)) + "/" +
        std::to_string(std::get<2>(tileLocation)) + ".png";
    return downloadSingleTile(middleTile, tileFilePath);
}

void showMiddleTile() {
    lv_obj_t * img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);

    lv_img_set_src(img1, "A:/middleTile.png");
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    Serial.println("Showing middle tile");
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

void deleteExistingFile(const char* tileFilePath) {
    if (FFat.exists(tileFilePath)) {
        Serial.println("Deleting existing tile...");
        FFat.remove(tileFilePath);
    }
}


void init_upload_log_page(lv_event_t * event)
{
    clearMainPage();
    Serial.println("Content:" + loadFileContent(logFilePath));
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Log File Content");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *logContentLabel = lv_label_create(mainPage);
    lv_label_set_text(logContentLabel, loadFileContent(logFilePath).c_str());
    lv_obj_align(logContentLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(logContentLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_t *uploadBtn = lv_btn_create(mainPage);
    lv_obj_align(uploadBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(uploadBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadBtn, upload_log_event_callback, LV_EVENT_CLICKED, NULL);


    lv_obj_t *uploadBtnLabel = lv_label_create(uploadBtn);
    lv_label_set_text(uploadBtnLabel, "Upload Log");
    lv_obj_center(uploadBtnLabel);

}

void upload_log_event_callback(lv_event_t * e)
{
    Serial.println("Starting sending log file");
    String logContent = loadFileContent(logFilePath);

    udp.begin(5050);

    udp.beginPacket("192.168.0.133", 5050);
    udp.write((const uint8_t *)logContent.c_str(), logContent.length());
    udp.endPacket();

    Serial.println("File sent over UDP.");

    init_main_menu();
    //create_popup(lv_scr_act());

}

void msgbox_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * mbox = lv_event_get_target(e);
        const char * btn_txt = lv_msgbox_get_active_btn_text(mbox);
        if(strcmp(btn_txt, "OK") == 0) {
            lv_obj_del_async(mbox);
            init_main_menu();
        }
    }
}

void create_popup(lv_obj_t * parent) {
    static const char * btns[] = {"OK", ""};
    
    lv_obj_t * mbox = lv_msgbox_create(parent, "Info", "Uploaded using UDP, port 5050!", btns, true);
    lv_obj_center(mbox);
    
    lv_obj_add_event_cb(mbox, msgbox_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
}

String loadFileContent(const char* filePath)
{
    File file = FFat.open(filePath, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading!");
        return "";
    }
    
    String fileContent;
    while (file.available()) {
        fileContent += (char)file.read();
    }

    file.close();

    return fileContent;
}

void initializeLoRa() {
    Serial.print(F("[SX1262] Receiver Initializing ... "));
    int state = lora.begin();
    if (state == RADIOLIB_ERR_NONE) {
        // Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    if (lora.setFrequency(433.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
        // Serial.println(F("Selected frequency is invalid for this module!"));
        while (true);
    }

    lora.setDio1Action(setFlag);
    // Serial.print(F("[SX1262] Starting to listen ... "));
    state = lora.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        // Serial.println(F("success!"));
    } else {
        // Serial.print(F("failed, code "));
        // Serial.println(state);
        while (true);
    }
}


void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    current_marker = NULL;
    soldiersNameLabel = NULL;

    deleteExistingFile(tileFilePath);
}

void init_main_poc_page(lv_event_t * event)
{
    Serial.println("init_main_poc_page");
    clearMainPage();

    deleteExistingFile(logFilePath);

    ballColor = lv_color_hex(0xff0000);
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Commander");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* waitingLabel = lv_label_create(mainPage);
    lv_obj_center(waitingLabel);
    lv_label_set_text(waitingLabel, "Waiting For Initial Coords");
    lv_obj_set_style_text_color(waitingLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

}

GPSCoordTuple parseCoordinates(const String &message) {
    float lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    sscanf(message.c_str(), "%f,%f;%f,%f", &lat1, &lon1, &lat2, &lon2);
    return std::make_tuple(lat1, lon1, lat2, lon2);
}

void init_p2p_test(lv_event_t * event)
{
    Serial.println("init_p2p_test");
    String incoming;
    int state = lora.readData(incoming);
    Serial.println("Received: " + incoming);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[SX1262] Received packet!"));
        GPSCoordTuple coords = parseCoordinates(incoming);

        float tile_lat   = std::get<0>(coords);
        float tile_lon   = std::get<1>(coords);
        float marker_lat = std::get<2>(coords);
        float marker_lon = std::get<3>(coords);

        struct tm timeInfo;
        char timeStr[9];

        if(getLocalTime(&timeInfo)) {
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
        } else {
            strcpy(timeStr, "00:00:00");
        }

        char result[128];

        snprintf(result, sizeof(result), "%s - Soldier 1: {%.5f, %.5f}", timeStr, tile_lat, tile_lon);
        Serial.println("SOME");
        writeToLogFile(logFilePath, result);
        Serial.println("SOME1");
        Serial.println(loadFileContent(logFilePath));
        Serial.println("SOME2");

        std::tuple<int, int, int> tileLocation = positionToTile(tile_lat, tile_lon, 19);
        
        if (!FFat.exists(tileFilePath)) {

            if (downloadMiddleTile(tileLocation)) {
                Serial.println("Middle tile downloaded.");
            } else {
                Serial.println("Failed to download middle tile.");
            }
        }

        if (!current_marker)
        {
            showMiddleTile();
        }
        
        
        int zoom, x_tile, y_tile;
        std::tie(zoom, x_tile, y_tile) = tileLocation;
        auto [centerLat, centerLon] = tileCenterLatLon(zoom, x_tile, y_tile);
        
        create_fading_red_circle(marker_lat, marker_lon, centerLat, centerLon, 19);

    }
    
    lora.startReceive();
}

void init_main_menu()
{   
    clearMainPage();

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Main Menu");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 240, 60);
    lv_obj_center(cont);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_flex_flow(cont, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(cont, lv_disp_get_hor_res(NULL), 60);

    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);

    lv_obj_t *receiveCoordsBtn = lv_btn_create(cont);
    lv_obj_center(receiveCoordsBtn);
    lv_obj_set_style_bg_color(receiveCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_set_style_flex_grow(receiveCoordsBtn, 1, 0);
    lv_obj_add_event_cb(receiveCoordsBtn, init_main_poc_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *receiveCoordsLabel = lv_label_create(receiveCoordsBtn);
    lv_label_set_text(receiveCoordsLabel, "Receive Pos");
    lv_obj_center(receiveCoordsLabel);


    lv_obj_t *uploadLogsBtn = lv_btn_create(cont);
    lv_obj_center(uploadLogsBtn);
    lv_obj_set_style_flex_grow(uploadLogsBtn, 1, 0);
    lv_obj_set_style_bg_color(uploadLogsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadLogsBtn, init_upload_log_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *uploadLogsLabel = lv_label_create(uploadLogsBtn);
    lv_label_set_text(uploadLogsLabel, "Upload Logs");
    lv_obj_center(uploadLogsLabel);
}



void writeToLogFile(const char* filePath, const char* content)
{
    File file = FFat.open(filePath, FILE_APPEND);

    if (!file) {
        Serial.println("Failed to open file for writing!");
    } else {
        
        file.println(content);
        file.close();
    }
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
    

    deleteExistingFile(tileFilePath);
    
    beginLvglHelper();
    lv_png_init();
    // Serial.println("LVGL set!");

    connectToWiFi();
    initializeLoRa();
    
    Serial.println("After LORA INIT");

    init_main_menu();

    watch.attachPMU(onPmuInterrupt);
    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();
    watch.enableIRQ(
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
        XPOWERS_AXP2101_PKEY_LONG_IRQ
    );
        

    Serial.println("End of setup");
}

void loop() {
    if (pmu_flag) {
        pmu_flag = false;
        uint32_t status = watch.readPMU();
        if (watch.isPekeyShortPressIrq()) {
            init_main_menu();
        }
        watch.clearPMU();
    }
  
    if (receivedFlag) {
        receivedFlag = false;
        init_p2p_test(nullptr);
    }
  
    lv_task_handler();
    delay(10);
}
