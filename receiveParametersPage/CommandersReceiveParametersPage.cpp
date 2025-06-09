#include "CommandersReceiveParametersPage.h"


CommandersReceiveParametersPage::CommandersReceiveParametersPage(std::unique_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);
    this->mainPage = lv_scr_act();
}

std::string CommandersReceiveParametersPage::extractPemBlock(const std::string& blob,
                                   const char* header,
                                   const char* footer)
{
    auto b = blob.find(header);
    auto e = (b == std::string::npos)
           ? std::string::npos
           : blob.find(footer, b);
    if (b == std::string::npos || e == std::string::npos) {
        throw std::runtime_error(std::string(header) + " … " + footer + " block not found");
    }
    e += std::strlen(footer);
    return blob.substr(b, e - b);
}

void CommandersReceiveParametersPage::createPage()
{
    lv_obj_t* vcontainer = lv_obj_create(this->mainPage);
    lv_obj_set_size(vcontainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vcontainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(vcontainer, LV_ALIGN_TOP_MID, 0, 20);

    this->openSocketButton = lv_btn_create(vcontainer);
    lv_obj_add_event_cb(this->openSocketButton, [](lv_event_t* e) {
        auto* self = static_cast<CommandersReceiveParametersPage*>(lv_event_get_user_data(e));
        if (self) 
        {
            lv_obj_add_flag(self->openSocketButton, LV_OBJ_FLAG_HIDDEN);
            self->onSocketOpened(e);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* open_label = lv_label_create(this->openSocketButton);
    lv_label_set_text(open_label, "Open Socket");
    lv_obj_center(open_label);

    this->uploadLogsButton = lv_btn_create(vcontainer);
    lv_obj_add_event_cb(this->uploadLogsButton, [](lv_event_t* e) {
        auto* self = static_cast<CommandersReceiveParametersPage*>(lv_event_get_user_data(e));
        if (self) {
            lv_obj_add_flag(self->uploadLogsButton, LV_OBJ_FLAG_HIDDEN);
            self->onMoveToUploadLogsPage(e);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* close_label = lv_label_create(this->uploadLogsButton);
    lv_label_set_text(close_label, "Upload Logs");
    lv_obj_center(close_label);

    for (int i = 0; i < sizeof(this->statusLabels) / sizeof(this->statusLabels[0]); ++i) {
        this->statusLabels[i] = lv_label_create(this->mainPage);
        lv_label_set_text(this->statusLabels[i], "");
        lv_obj_align(this->statusLabels[i], LV_ALIGN_TOP_MID, 0, 60 + i * 30);
    }
}



void CommandersReceiveParametersPage::setOnReceiveParams(std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Commander>)> cb) {
    this->onReceiveParams = std::move(cb);
}

void CommandersReceiveParametersPage::setOnUploadLogs(std::function<void(std::unique_ptr<WifiModule>)> cb)
{
  this->onUploadLogs = std::move(cb);
}

void CommandersReceiveParametersPage::onSocketOpened(lv_event_t* event)
{
  Serial.println("onSocketOpened");
  // TCP SSL

  FFatHelper::deleteFile(this->logFilePath.c_str());
  JsonDocument doc;
  doc = this->wifiModule->receiveJSONTCP("192.168.0.181", 8743);

  try {

    const std::string combinedPem = doc["certificate"].as<std::string>();


    std::string ownCertPem = extractPemBlock(
      combinedPem,
      "-----BEGIN CERTIFICATE-----",
      "-----END CERTIFICATE-----"
    );

    std::string privateKeyPem = extractPemBlock(
      combinedPem,
      "-----BEGIN RSA PRIVATE KEY-----",
      "-----END RSA PRIVATE KEY-----"
    );

    mbedtls_x509_crt ownCert;
    mbedtls_x509_crt_init(&ownCert);
    if (mbedtls_x509_crt_parse(&ownCert, (const unsigned char*)ownCertPem.c_str(), ownCertPem.size()+1) != 0)
      throw std::runtime_error("Failed to parse server certificate");
    
    updateLabel(0);


    const std::string caPem = doc["caCertificate"].as<std::string>();
    mbedtls_x509_crt caCert;
    mbedtls_x509_crt_init(&caCert);
    if (mbedtls_x509_crt_parse(&caCert,
          (const unsigned char*)caPem.c_str(), caPem.size()+1) != 0)
      throw std::runtime_error("Failed to parse CA certificate");

    updateLabel(1);

    const std::string gmkStr = doc["gmk"].as<std::string>();
    crypto::Key256 GMK = crypto::CryptoModule::strToKey256(gmkStr);

    updateLabel(2);
    
    const std::string missionID = doc["mission"].as<std::string>();
    
    std::vector<float> freqs;
    for (auto v : doc["frequencies"].as<JsonArray>())
      freqs.push_back(v.as<float>());

    updateLabel(3);

    const uint16_t intervalMs = doc["intervalMs"].as<uint16_t>();

    updateLabel(4);

    mbedtls_pk_context privKey;
    mbedtls_pk_init(&privKey);
    if (mbedtls_pk_parse_key(&privKey, (const unsigned char*)privateKeyPem.c_str(), privateKeyPem.size()+1, nullptr, 0) != 0)
      throw std::runtime_error("Failed to parse private key");

    NameId ownNi = certModule::parseNameIdFromCertPem(ownCertPem);

    this->commanderModule = std::make_unique<Commander>(
        ownNi.name,
        ownCert,
        privKey,
        caCert,
        ownNi.id,
        intervalMs
    );

    this->commanderModule->appendFrequencies(freqs);

  
    for (auto v : doc["soldiers"].as<JsonArray>()) {
        const std::string pem = v.as<std::string>();

        SoldierInfo soldInfo;
        mbedtls_x509_crt_init(&soldInfo.cert);
        if (mbedtls_x509_crt_parse(&soldInfo.cert, reinterpret_cast<const unsigned char*>(pem.c_str()), pem.size() + 1) != 0)
        {
            mbedtls_x509_crt_free(&soldInfo.cert);
            continue;
        }

        try {
            NameId ni = certModule::parseNameIdFromCertPem(pem);
            soldInfo.name = std::move(ni.name);
            soldInfo.soldierNumber = ni.id;
            soldInfo.status = SoldiersStatus::REGULAR;
            soldInfo.lastTimeReceivedData = millis();
        }
        catch (...) {
            mbedtls_x509_crt_free(&soldInfo.cert);
            continue;
        }

        this->commanderModule->addSoldier(std::move(soldInfo));
    }

    for (auto v : doc["commanders"].as<JsonArray>()) {
        const std::string pem = v.as<std::string>();

        CommanderInfo commandInfo;
        mbedtls_x509_crt_init(&commandInfo.cert);
        if (mbedtls_x509_crt_parse(&commandInfo.cert, reinterpret_cast<const unsigned char*>(pem.c_str()), pem.size() + 1) != 0)
        {
            mbedtls_x509_crt_free(&commandInfo.cert);
            continue;
        }

        try {
            NameId ni = certModule::parseNameIdFromCertPem(pem);
            commandInfo.name = std::move(ni.name);
            commandInfo.commanderNumber = ni.id;
            commandInfo.status = SoldiersStatus::REGULAR;
            commandInfo.lastTimeReceivedData = millis();
        }
        catch (...) {
            mbedtls_x509_crt_free(&commandInfo.cert);
            continue;
        }

        this->commanderModule->addCommander(std::move(commandInfo));
    }
    updateLabel(5);


    mbedtls_x509_crt_free(&ownCert);
    mbedtls_x509_crt_free(&caCert);
    mbedtls_pk_free(&privKey);

    if (this->commanderModule) {
        Serial.println("Soldier parameters:");
        Serial.print("Name: ");
        Serial.println(this->commanderModule->getName().c_str());
        Serial.print("Soldier Number: ");
        Serial.println(this->commanderModule->getCommanderNumber());
        Serial.print("Current Heart Rate: ");
        Serial.println(this->commanderModule->getCurrentHeartRate());
        Serial.print("Frequencies count: ");
        Serial.println(this->commanderModule->getFrequencies().size());
    }

    Serial.println("Certificates and Private Key:");
    Serial.println("Own Certificate PEM:");
    Serial.println(ownCertPem.c_str());
    Serial.println("Private Key PEM:");
    Serial.println(privateKeyPem.c_str());
    Serial.println("CA Certificate PEM:");
    Serial.println(caPem.c_str());

    File certFile = FFat.open(this->certFile.c_str(), FILE_WRITE);
    certFile.print(ownCertPem.c_str());
    certFile.close();

    File CAcertFile = FFat.open(this->caCertPath.c_str(), FILE_WRITE);
    CAcertFile.print(caPem.c_str());
    CAcertFile.close();

    String missionIDStr = String(missionID.c_str());
    FFatHelper::initializeLogFile(this->logFilePath.c_str(), this->commanderModule->getIntervalMS(), missionIDStr);
    Serial.println("After init");
    this->destroyPage();
    delay(10);

    Serial.printf("GMK: %s\n", crypto::CryptoModule::key256ToAsciiString(GMK).c_str());

    Serial.println("AFTER destroyPage");
    if (this->onReceiveParams) {
      Serial.println("onReceiveParams Exists");
      this->onReceiveParams(std::move(this->wifiModule), std::move(this->commanderModule));
    }
    else
    {
      Serial.println("onReceiveParams Doesn't exist");
    }
  }
  catch (const std::exception &e) {
    Serial.printf("⚠️ onSocketOpened error: %s\n", e.what());
  }
}

void CommandersReceiveParametersPage::onMoveToUploadLogsPage(lv_event_t* event)
{
  this->destroyPage();

  delay(10);

  this->onUploadLogs(std::move(this->wifiModule));

}

void CommandersReceiveParametersPage::updateLabel(uint8_t index)
{
    lv_label_set_text(this->statusLabels[index], this->messages[index]);
}

