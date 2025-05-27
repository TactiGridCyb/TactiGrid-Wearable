#include "UploadLogPage.h"
#include "lvgl.h"
#include <fstream>
#include <sstream>

UploadLogPage::UploadLogPage(std::shared_ptr<WifiModule> wifiModule, const std::string& logFilePath)
    : wifiModule(std::move(wifiModule)), logFilePath(logFilePath) {}

void UploadLogPage::createPage() {
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Log File Content");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *logContentLabel = lv_label_create(mainPage);
    std::string content = loadFileContent(logFilePath);
    lv_label_set_text(logContentLabel, content.c_str());
    lv_obj_align(logContentLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(logContentLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(logContentLabel, lv_disp_get_hor_res(NULL) - 20);
    lv_label_set_long_mode(logContentLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t *uploadBtn = lv_btn_create(mainPage);
    lv_obj_align(uploadBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(uploadBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadBtn, upload_log_event_callback, LV_EVENT_CLICKED, this);

    lv_obj_t *uploadBtnLabel = lv_label_create(uploadBtn);
    lv_label_set_text(uploadBtnLabel, "Upload Log");
    lv_obj_center(uploadBtnLabel);
}

void UploadLogPage::upload_log_event_callback(lv_event_t* e) {
    auto* page = static_cast<UploadLogPage*>(lv_event_get_user_data(e));
    if (!page) return;
    std::string logContent = page->loadFileContent(page->logFilePath);
    page->wifiModule->sendString(logContent.c_str(), "192.168.0.44", 5555);
    // TODO: Replace with navigation to main menu page
}

std::string UploadLogPage::loadFileContent(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    if (file) {
        buffer << file.rdbuf();
    }
    return buffer.str();
}