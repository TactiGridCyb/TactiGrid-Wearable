#pragma once
#include "LVGLPage.h"
#include "WifiModule.h"
#include "../env.h"
#include "lvgl.h"
#include <string>
#include <memory>
#include <CryptoModule.h>
#include <certModule.h>

class CommandersUploadLogPage : public LVGLPage {
public:
    CommandersUploadLogPage(std::shared_ptr<WifiModule> wifiModule, const std::string& logFilePath);

    void createPage() override;

private:
    std::shared_ptr<WifiModule> wifiModule;
    std::string logFilePath;

    const char* encLogPath = "/encLog.txt";
    const char* certPath = "/cert.txt";
    const char* caCertPath = "/CAcert.txt";
    const char* encKeyPath = "/encKey.txt";
    const char* finalPayload = "/uploadPayload.txt";
        

    static void upload_log_event_callback(lv_event_t* e);
};