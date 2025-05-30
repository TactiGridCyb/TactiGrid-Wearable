#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

#include <tuple>
#include <memory>
#include <FFat.h>
#include <FS.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <LV_Helper.h>
#include <LilyGoLib.h>
#include <cstring>
#include <vector>

#include "../encryption/CryptoModule.h"
#include "../LoraModule/LoraModule.h"
#include "../wifi-connection/WifiModule.h"
#include "../envLoader.cpp"
#include "../soldier/SoldiersSentData.h"
#include "../FHFModule/FHFModule.h"
#include "../env.h"

const crypto::Key256 SHARED_KEY = []() {
    crypto::Key256 key{};

    const char* raw = "0123456789abcdef0123456789abcdef"; 
    std::memcpy(key.data(), raw, 32);
    return key;
}();


lv_obj_t *current_marker = NULL;
lv_obj_t *current_second_marker = NULL;

std::unique_ptr<LoraModule> loraModule;
std::unique_ptr<WifiModule> wifiModule;
std::unique_ptr<FHFModule> fhfModule;

const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
const char* tileFilePath = "/middleTile.png";
const char* logFilePath = "/log.txt";

using GPSCoordTuple = std::tuple<float, float, float, float>;

volatile bool pmu_flag = false;

lv_obj_t* soldiersNameLabel = NULL;
lv_obj_t* secondSoldiersNameLabel = NULL;


const std::vector<float> freqList = {
    433.5, 433.6, 433.7, 433.8
};

const uint32_t hopIntervalSeconds = 30;


float currentLoraFreq = 433.5f;

static inline crypto::ByteVec hexToBytes(const String& hex) {
    crypto::ByteVec out;
    out.reserve(hex.length() >> 1);
    for (int i = 0; i < hex.length(); i += 2) {
        out.push_back(strtoul(hex.substring(i, i + 2).c_str(), nullptr, 16));
    }
    return out;
}

lv_color_t ballColor = lv_color_hex(0xff0000);
lv_color_t secondBallColor = lv_color_hex(0xff0000);

void notifySoldiersNotAvailable() {
    const char* msg = "CMD_BUSY";
    loraModule->cancelReceive();
    int16_t status = loraModule->sendData(msg);
    Serial.printf("Notify soldiers not available, send status: %d\n", status);
}

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


void create_fading_circle(double markerLat, double markerLon, double centerLat, double centerLon, uint16_t soldiersID, int zoom,
     lv_color_t* ballColor, lv_obj_t*& marker, lv_obj_t*& label) {

    if (marker != NULL) {
        lv_obj_del(marker);
        marker = NULL;
    }

    int pixel_x, pixel_y;
    std::tie(pixel_x, pixel_y) = latlon_to_pixel(markerLat, markerLon, centerLat, centerLon, zoom);

    marker = lv_obj_create(lv_scr_act());
    lv_obj_set_size(marker, 10, 10);
    lv_obj_set_style_bg_color(marker, *ballColor, 0);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_pos(marker, pixel_x, pixel_y);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, marker);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_opacity_cb);
    lv_anim_start(&a);

    if(!label)
    {
        label = lv_label_create(lv_scr_act());

        std::string s = std::to_string(soldiersID);
        lv_label_set_text(label, s.c_str());
    }
    
    
    lv_obj_align_to(label, marker, LV_ALIGN_TOP_MID, 0, -35);
}

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    current_marker = NULL;
    current_second_marker = NULL;
    soldiersNameLabel = NULL;
    secondSoldiersNameLabel = NULL;

    deleteExistingFile(tileFilePath);
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
        secondBallColor = lv_color_hex(0x000000);
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

    // Serial.println("Showing middle tile");
}


void deleteExistingFile(const char* tileFilePath) {
    if (FFat.exists(tileFilePath)) {
        // Serial.println("Deleting existing tile...");
        FFat.remove(tileFilePath);
    }
}


void init_upload_log_page(lv_event_t * event)
{
    clearMainPage();
    // Serial.println("Content:" + loadFileContent(logFilePath));
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
    
    lv_obj_set_width(logContentLabel, lv_disp_get_hor_res(NULL) - 20);
    lv_label_set_long_mode(logContentLabel, LV_LABEL_LONG_WRAP);

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
    // Serial.println("Starting sending log file");
    String logContent = loadFileContent(logFilePath);

    wifiModule->sendString(logContent.c_str(), "192.168.0.44", 5555);

    // Serial.println("File sent over UDP.");

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
    
    lv_obj_t * mbox = lv_msgbox_create(parent, "Info", "Uploaded using UDP, port 5555!", btns, true);
    lv_obj_center(mbox);
    
    lv_obj_add_event_cb(mbox, msgbox_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
}

String loadFileContent(const char* filePath)
{
    File file = FFat.open(filePath, FILE_READ);
    if (!file) {
        // Serial.println("Failed to open file for reading!");
        return "";
    }
    
    String fileContent;
    while (file.available()) {
        fileContent += (char)file.read();
    }

    file.close();

    return fileContent;
}

void listFiles(const char* path = "/", uint8_t depth = 0) {
    File dir = FFat.open(path);
    if (!dir || !dir.isDirectory()) {
        // Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        for (uint8_t i = 0; i < depth; i++) Serial.print("  ");

        if (file.isDirectory()) {
            // Serial.printf("[DIR]  %s\n", file.name());

            listFiles(file.name(), depth + 1);
        } else {
            // Serial.printf("FILE:  %s  SIZE: %d\n", file.name(), file.size());
        }

        file = dir.openNextFile();
    }
}




void init_main_poc_page(lv_event_t * event)
{
    // Serial.println("init_main_poc_page");
    clearMainPage();

    deleteExistingFile(logFilePath);

    ballColor = lv_color_hex(0xff0000);
    secondBallColor = lv_color_hex(0xff0000);

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

bool isZero(double x)
{
    return std::fabs(x) < 1e-9;
}

void init_p2p_test(const uint8_t* data, size_t len)
{
    String incoming;
    for (size_t i = 0; i < len; i++) {
        incoming += (char)data[i];
    }

    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        // Serial.println("Bad ciphertext format");
        return;
    }

    crypto::Ciphertext ct;
    ct.nonce = hexToBytes(incoming.substring(0, p1));
    ct.data  = hexToBytes(incoming.substring(p1 + 1, p2));
    ct.tag   = hexToBytes(incoming.substring(p2 + 1));

    // Serial.printf("Nonce size: %d\n", ct.nonce.size());
    // Serial.printf("Data size:  %d\n", ct.data.size());
    // Serial.printf("Tag size:   %d\n", ct.tag.size());
    
    crypto::ByteVec pt;

    try {
        pt = crypto::CryptoModule::decrypt(SHARED_KEY, ct);
    } catch (const std::exception& e) {
        // Serial.printf("Decryption failed: %s\n", e.what());
        return;
    }


    SoldiersSentData* newG = reinterpret_cast<SoldiersSentData*>(pt.data());
    Serial.printf("%.5f %.5f %.5f %.5f %d\n", newG->tileLon, newG->tileLat, newG->posLat, newG->posLon, newG->soldiersID);
    String plainStr;
    plainStr.reserve(pt.size());
    for (unsigned char b : pt) plainStr += (char)b;

    Serial.println("Decrypted: " + plainStr);

    GPSCoordTuple coords = parseCoordinates(plainStr);

    float tile_lat = newG->tileLat;
    float tile_lon = newG->tileLon;
    float marker_lat = newG->posLat;
    float marker_lon = newG->posLon;

    if(isZero(tile_lat) || isZero(tile_lon) || isZero(marker_lat) || isZero(marker_lon))
    {
        return;
    }
    
    struct tm timeInfo;
    char timeStr[9];
    if (getLocalTime(&timeInfo))
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
    else
        strcpy(timeStr, "00:00:00");

    char result[128];
    snprintf(result, sizeof(result), "%s - Soldier 1: {%.5f, %.5f}", 
             timeStr, tile_lat, tile_lon);
    writeToLogFile(logFilePath, result);

    std::tuple<int,int,int> tileLocation = positionToTile(tile_lat, tile_lon, 19);

    if (!FFat.exists(tileFilePath) && downloadMiddleTile(tileLocation))
        // Serial.println("Middle tile downloaded.");

    if (!current_marker && !current_second_marker)
        showMiddleTile();

    auto [zoom, x_tile, y_tile] = tileLocation;
    auto [centerLat, centerLon] = tileCenterLatLon(zoom, x_tile, y_tile);

    int heartRate = newG->heartRate;
    if(newG->soldiersID == 1)
    {
        ballColor = getColorFromHeartRate(heartRate);
        create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, newG->soldiersID, 19, &ballColor, current_marker, soldiersNameLabel);
    }
    else
    {
        secondBallColor = getColorFromHeartRate(heartRate);
        create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, newG->soldiersID, 19, &secondBallColor, current_second_marker, secondSoldiersNameLabel);
    }
    
    if(newG->heartRate == 0)
    {
        Serial.println("compromisedEvent");
        compromisedEvent();
        init_main_menu();
    }
    

}

void compromisedEvent()
{
    loraModule->switchToTransmitterMode();

    for(int i = 0; i < 10; ++i)
    {
        notifySoldiersNotAvailable();
        delay(200);
    }
}

void init_receive_logs_page(lv_event_t * event)
{
    Serial.println("loraModule->setupListening()");
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

    lv_obj_t *receiveFile = lv_btn_create(mainPage);
    lv_obj_set_size(receiveFile, 240, 60);
    lv_obj_set_style_bg_color(receiveFile, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(receiveFile, init_receive_logs_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *receiveFileLabel = lv_label_create(receiveFile);
    lv_label_set_text(receiveFileLabel, "Receive Logs");
    lv_obj_center(receiveFileLabel);

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


static std::vector<uint8_t> receivedFileBuffer;
static size_t expectedFileLength = 0;
static size_t expectedChunkSize = 0;
static uint16_t lastReceivedChunk = 0xFFFF;
static bool receivingFile = false;

static void dumpHex(const uint8_t* buf, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
}

void finishTransfer()
{
    receivingFile = false;

    Serial.printf("\n--- received %u bytes ---\n", receivedFileBuffer.size());
    Serial.write(receivedFileBuffer.data(), receivedFileBuffer.size());
    Serial.println("\n-------------------------");

    dumpHex(receivedFileBuffer.data(), receivedFileBuffer.size());
}

void parseInitFrame(const char* pkt, size_t len)
{

    const size_t tagLen = strlen(kFileInitTag);
    if (len <= tagLen + 1 || pkt[tagLen] != ':') {
        Serial.println("INIT frame corrupted");
        return;
    }

    size_t fileLen    = 0;
    size_t chunkBytes = 0;
    int parsed = sscanf(pkt + tagLen, ":%zu:%zu", &fileLen, &chunkBytes);
    if (parsed != 2 || fileLen == 0 || chunkBytes == 0) {
        Serial.println("INIT numbers invalid");
        return;
    }

    expectedFileLength = fileLen;
    expectedChunkSize  = chunkBytes;

    receivedFileBuffer.clear();
    receivedFileBuffer.reserve(expectedFileLength);
    lastReceivedChunk  = 0xFFFF;
    receivingFile      = true;

    Serial.printf("INIT  fileLen=%u  chunkSize=%u\n",
                  (unsigned)expectedFileLength,
                  (unsigned)expectedChunkSize);
}

void onLoraFileDataReceived(const uint8_t* pkt, size_t len)
{
    Serial.println(len);
    if (len == 0) return;

    if (memcmp(pkt, kFileInitTag, strlen(kFileInitTag)) == 0) {
        parseInitFrame((const char*)pkt, len);
        return;
    }
    if (memcmp(pkt, kFileEndTag, strlen(kFileEndTag)) == 0) {
        finishTransfer();
        return;
    }

    handleFileChunk(pkt, len);
}

void handleFileChunk(const uint8_t* bytes, size_t len)
{   
    Serial.println("handleFileChunk");
    Serial.println(len);
    Serial.println(bytes[0]);

    if (len < 4 || bytes[0] != 0xAB) return;

    uint16_t chunkNum = (bytes[1] << 8) | bytes[2];
    uint8_t  chunkLen = bytes[3];

    Serial.println(chunkNum);
    Serial.println(chunkLen);

    if (chunkLen + 4 != len) return;         
    if (chunkNum != lastReceivedChunk + 1 && lastReceivedChunk != 0xFFFF)
        return;

    lastReceivedChunk = chunkNum;
    receivedFileBuffer.insert(receivedFileBuffer.end(),
                              bytes + 4, bytes + 4 + chunkLen);

    Serial.printf("chunk=%u len=%u total=%u/%u\n",
                  chunkNum, chunkLen,
                  receivedFileBuffer.size(), expectedFileLength);

    String chunkTxt((const char*)(bytes + 4), chunkLen);
    Serial.println(chunkTxt);
}

void writeToLogFile(const char* filePath, const char* content)
{
    File file = FFat.open(filePath, FILE_APPEND);

    if (!file) {
        // Serial.println("Failed to open file for writing!");
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

    
    listFiles();
    load_env();

    Serial.println("load_env");

    crypto::CryptoModule::init();
    Serial.println("init cryptoModule");
    

    deleteExistingFile(tileFilePath);


    beginLvglHelper();
    lv_png_init();
    Serial.println("LVGL set!");


    const char* ssid = WIFI_SSID;
    const char* password = WIFI_PASS;

    wifiModule = std::make_unique<WifiModule>(ssid, password);
    wifiModule->connect(10000);

    loraModule = std::make_unique<LoraModule>(433.5);
    loraModule->setup(false);
    loraModule->setOnReadData(init_p2p_test);
    
    fhfModule = std::make_unique<FHFModule>(freqList, hopIntervalSeconds);
    currentLoraFreq = 433.5;

    Serial.println("After LORA INIT");

    init_main_menu();

    Serial.println(WiFi.localIP());
    setenv("TZ", "GMT-3", 1);
    tzset();

    configTime(0, 0, "pool.ntp.org");

    Serial.println("Configured time zone!");


    struct tm timeInfo;

    while (!getLocalTime(&timeInfo)) {
        Serial.println("Waiting for time sync...");
        delay(500);
    }

    if (getLocalTime(&timeInfo)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
        Serial.println(timeStr);
    }

    watch.attachPMU(onPmuInterrupt);
    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();
    watch.enableIRQ(
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
        XPOWERS_AXP2101_PKEY_LONG_IRQ
    );
        
    Serial.println("End of setup");
    loraModule->readData();
}

static unsigned long lastFreqCheck = 0;

void loop() {
    if (pmu_flag) {
        pmu_flag = false;
        uint32_t status = watch.readPMU();
        if (watch.isPekeyShortPressIrq()) {
            init_main_menu();
        }
        watch.clearPMU();
    }
  
    loraModule->handleCompletedOperation();

    // unsigned long now = millis();
    // if (!loraModule->isBusy() &&
    //     now - lastFreqCheck >= 1000UL)
    // {
    //     lastFreqCheck = now;
    //     float newFreq = fhfModule->currentFrequency();
    //     if (!isZero(newFreq - currentLoraFreq)) {
    //         currentLoraFreq = newFreq;
    //         loraModule->setFrequency(currentLoraFreq);
    //         Serial.printf("Frequency hopped to %.3f MHz\n", currentLoraFreq);
    //     }
    // }

    if (!loraModule->isBusy()) {
        loraModule->readData();
    }

    lv_task_handler();
    delay(10);
}
