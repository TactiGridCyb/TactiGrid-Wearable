#include "SoldiersReceiveParametersPage.h"


SoldiersReceiveParametersPage::SoldiersReceiveParametersPage(std::unique_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);
    this->mainPage = lv_scr_act();
}

std::string SoldiersReceiveParametersPage::extractPemBlock(const std::string& blob,
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

void SoldiersReceiveParametersPage::createPage()
{
    this->openSocketButton = lv_btn_create(this->mainPage);
    lv_obj_align(this->openSocketButton, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(this->openSocketButton, [](lv_event_t* e) {
        auto* self = static_cast<SoldiersReceiveParametersPage*>(lv_event_get_user_data(e));
        if (self) {
            lv_obj_add_flag(self->openSocketButton, LV_OBJ_FLAG_HIDDEN);
            self->onSocketOpened(e);
        }
    },
    LV_EVENT_CLICKED, this);
    
    lv_obj_t *btn_label = lv_label_create(this->openSocketButton);
    lv_label_set_text(btn_label, "Open Socket");
    lv_obj_center(btn_label);

    for (int i = 0; i < sizeof(this->statusLabels) / sizeof(this->statusLabels[0]); ++i) {
        this->statusLabels[i] = lv_label_create(this->mainPage);
        lv_label_set_text(this->statusLabels[i], "");
        lv_obj_align(this->statusLabels[i], LV_ALIGN_TOP_MID, 0, 60 + i * 30);
    }
}

#include <functional>

void SoldiersReceiveParametersPage::setOnTransferPage(std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Soldier>)> cb) {
    this->onTransferPage = std::move(cb);
}

void SoldiersReceiveParametersPage::onSocketOpened(lv_event_t* event)
{
  Serial.println("onSocketOpened");
  // TCP SSL
  JsonDocument doc;
  doc = this->wifiModule->receiveJSONTCP("192.168.113.1", 8743);

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

    this->soldierModule = std::make_unique<Soldier>(
        ownNi.name,
        ownCert,
        privKey,
        caCert,
        ownNi.id,
        intervalMs
    );

    this->soldierModule->appendFrequencies(freqs);

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

        this->soldierModule->addSoldier(std::move(soldInfo));
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

        this->soldierModule->addCommander(std::move(commandInfo));
    }

    updateLabel(5);

    Serial.println("Private Key PEM:");
    Serial.println(certModule::privateKeyToString(privKey).c_str());
    
    mbedtls_x509_crt_free(&ownCert);
    mbedtls_x509_crt_free(&caCert);
    mbedtls_pk_free(&privKey);


    if (this->soldierModule) {
        Serial.println("Soldier parameters:");
        Serial.print("Name: ");
        Serial.println(this->soldierModule->getName().c_str());
        Serial.print("Soldier Number: ");
        Serial.println(this->soldierModule->getSoldierNumber());
        Serial.print("Current Heart Rate: ");
        Serial.println(this->soldierModule->getCurrentHeartRate());
        Serial.print("Frequencies count: ");
        Serial.println(this->soldierModule->getFrequencies().size());
    }

    Serial.println("Certificates and Private Key:");
    Serial.println("Own Certificate PEM:");
    Serial.println(ownCertPem.c_str());

    Serial.println("CA Certificate PEM:");
    Serial.println(caPem.c_str());

    this->destroyPage();
    Serial.printf("GMK: %s\n", crypto::CryptoModule::key256ToAsciiString(GMK).c_str());

    delay(10);
    Serial.println("AFTER destroyPage");
    if (this->onTransferPage) {
      Serial.println("onTransferPage Exists");
      this->onTransferPage(std::move(this->wifiModule), std::move(this->soldierModule));
    }
    else
    {
      Serial.println("onTransferPage Doesn't exist");
    }
  }
  catch (const std::exception &e) {
    Serial.printf("⚠️ onSocketOpened error: %s\n", e.what());
  }
}

void SoldiersReceiveParametersPage::updateLabel(uint8_t index)
{
    lv_label_set_text(this->statusLabels[index], this->messages[index]);
}

