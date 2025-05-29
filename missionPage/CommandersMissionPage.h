#pragma once

#include <LVGLPage.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>
#include <string>
#include <memory>
#include <CryptoModule.h>
#include <FFatHelper.h>

class CommandersMissionPage : public LVGLPage {
public:
    CommandersMissionPage(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>, std::shared_ptr<GPSModule>, bool = true, const std::string&);

    void createPage() override;

    static inline crypto::ByteVec hexToBytes(const String& hex);

private:
    std::shared_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;

    std::string logFilePath;

    crypto::Key256 sharedKey;

    bool fakeGPS;


    lv_obj_t* mainPage;

    std::unordered_map<uint16_t, lv_color_t> ballColors;
    std::unordered_map<uint16_t, lv_obj_t*> labels;

    void onDataReceived(const uint8_t* data, size_t len);
};