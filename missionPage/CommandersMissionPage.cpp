#include "CommandersMissionPage.h"

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule,
     std::shared_ptr<GPSModule> gpsModule, bool fakeGPS = true, const std::string& logFilePath)
    : logFilePath(logFilePath) 
    {
        this->loraModule = std::move(loraModule);
        this->wifiModule = std::move(wifiModule);
        this->gpsModule = std::move(gpsModule);

        this->mainPage = lv_scr_act();
    }

void CommandersMissionPage::createPage() {
    FFatHelper::deleteFile(this->logFilePath.c_str());

    lv_color_t ballColor = lv_color_hex(0xff0000);
    lv_color_t secondBallColor = lv_color_hex(0xff0000);

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

void CommandersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{
    String incoming;
    for (size_t i = 0; i < len; i++) 
    {
        incoming += (char)data[i];
    }

    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        Serial.println("Bad ciphertext format");
        return;
    }

    crypto::Ciphertext ct;
    ct.nonce = hexToBytes(incoming.substring(0, p1));
    ct.data  = hexToBytes(incoming.substring(p1 + 1, p2));
    ct.tag   = hexToBytes(incoming.substring(p2 + 1));
}

inline crypto::ByteVec CommandersMissionPage::hexToBytes(const String& hex) 
{
    crypto::ByteVec out;
    out.reserve(hex.length() >> 1);
    for (int i = 0; i < hex.length(); i += 2) {
        out.push_back(strtoul(hex.substring(i, i + 2).c_str(), nullptr, 16));
    }
    return out;
}