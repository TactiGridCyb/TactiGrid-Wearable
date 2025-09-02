#include "CommandersUploadLogPage.h"


CommandersUploadLogPage::CommandersUploadLogPage(std::shared_ptr<WifiModule> wifiModule, const std::string& logFilePath)
    : wifiModule(std::move(wifiModule)), logFilePath(logFilePath) 
    {

    }

void CommandersUploadLogPage::createPage() {
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Log File Content");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *logContentLabel = lv_label_create(mainPage);
    String content;

    FFatHelper::readFile(this->logFilePath.c_str(), content);

    Serial.println(content.c_str());

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

void CommandersUploadLogPage::upload_log_event_callback(lv_event_t* e) {
    auto* page = static_cast<CommandersUploadLogPage*>(lv_event_get_user_data(e));
    if (!page) return;

    crypto::Key256 currentKey = crypto::CryptoModule::generateGMK();
    
    crypto::Ciphertext ct = crypto::CryptoModule::encryptFile(currentKey, page->logFilePath.c_str());
    Serial.printf("current gmk generated:%s \n%d\n", crypto::CryptoModule::keyToHex(currentKey).c_str(), ct.nonce.size());
    
    File out = FFat.open(page->encLogPath, FILE_WRITE);

    out.write(ct.nonce.data(), ct.nonce.size());
    out.write(ct.data.data(), ct.data.size());
    out.write(ct.tag.data(), ct.tag.size());

    out.close();

    String caCert;
    String ownCert;

    FFatHelper::readFile(page->caCertPath, caCert);
    FFatHelper::readFile(page->certPath, ownCert);
    std::string pemCert(caCert.c_str());
    std::string ownStrCert(ownCert.c_str());

    Serial.printf("OWN CERT:%s\n",ownStrCert.c_str());
    Serial.printf("CA CERT:%s\n",pemCert.c_str());

    std::string keyData(reinterpret_cast<const char*>(currentKey.data()), currentKey.size());
    std::vector<uint8_t> encryptedKey;

    certModule::encryptWithPublicKeyPem(pemCert, keyData, encryptedKey);
    
    File f = FFat.open(page->encKeyPath, FILE_WRITE);
    if (!f) 
    {
        Serial.println("❌ Failed to open encrypted key file");
        return;
    }
    f.write(encryptedKey.data(), encryptedKey.size());
    f.close();

    File keyFile = FFat.open(page->encKeyPath, FILE_READ);
    std::vector<uint8_t> keyBuf(keyFile.size());
    keyFile.read(keyBuf.data(), keyBuf.size());
    keyFile.close();

    File logFile = FFat.open(page->encLogPath, FILE_READ);
    std::vector<uint8_t> logBuf(logFile.size());
    logFile.read(logBuf.data(), logBuf.size());
    logFile.close();

    std::string encKeyB64 = crypto::CryptoModule::base64Encode(keyBuf.data(), keyBuf.size());
    std::string encLogB64 = crypto::CryptoModule::base64Encode(logBuf.data(), logBuf.size());

    Serial.printf("enc64: %s\n encLog64: %s\n", encKeyB64.c_str(), encLogB64.c_str());

    String payload;
    JsonDocument doc;

    doc["certificatePem"] = ownStrCert;
    doc["gmk"] = encKeyB64;
    doc["log"] = encLogB64;

    serializeJson(doc, payload);
    Serial.printf("Final json\n%s\n", payload.c_str());

    logFile = FFat.open(page->logFilePath.c_str(), FILE_READ);

    JsonDocument logJSON;

    DeserializationError error = deserializeJson(logJSON, logFile);
    logFile.close();
    
    if (error) 
    {
        Serial.print("❌ Failed to parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    const std::string missionID = logJSON["Mission"].as<std::string>();
    String url = "http://" + String(WEBAPP_IP) + ":3000/upload/" + String(missionID.c_str());
    Serial.printf("URL IS: %s\n", url.c_str());
    page->wifiModule->sendStringPost(url.c_str(), payload, 8743);


}

