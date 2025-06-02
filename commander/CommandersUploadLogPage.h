#pragma once
#include "LVGLPage.h"
#include "WifiModule.h"
#include "lvgl.h"
#include <string>
#include <memory>

class CommandersUploadLogPage : public LVGLPage {
public:
    CommandersUploadLogPage(std::shared_ptr<WifiModule> wifiModule, const std::string& logFilePath);

    void createPage() override;

private:
    std::shared_ptr<WifiModule> wifiModule;
    std::string logFilePath;

    static void upload_log_event_callback(lv_event_t* e);
};