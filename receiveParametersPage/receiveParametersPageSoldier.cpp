#include "receiveParametersPageSoldier.h"
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <stdexcept>
#include <string>
#include <regex>

receiveParametersPageSoldier::receiveParametersPageSoldier(std::unique_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);
    this->mainPage = lv_scr_act();
}

void receiveParametersPageSoldier::createPage()
{
    lv_obj_t *btn = lv_btn_create(this->mainPage);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* self = static_cast<receiveParametersPageSoldier*>(lv_event_get_user_data(e));
        if (self) self->onSocketOpened(e);
    },
    LV_EVENT_CLICKED, this);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Open Socket");
    lv_obj_center(btn_label);

    for (int i = 0; i < sizeof(this->statusLabels) / sizeof(this->statusLabels[0]); ++i) {
        this->statusLabels[i] = lv_label_create(this->mainPage);
        lv_label_set_text(this->statusLabels[i], "");
        lv_obj_align(this->statusLabels[i], LV_ALIGN_TOP_MID, 0, 60 + i * 30);
    }
}

void receiveParametersPageSoldier::destroy()
{
    lv_obj_t* mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);
}
 
void receiveParametersPageSoldier::onSocketOpened(lv_event_t* event)
{
  // TCP SSL
  JsonDocument doc;
  doc = this->wifiModule->receiveJSONTCP("127.0.0.1", 8743);

  try {
    const std::string combinedPem = doc["certificate"].as<std::string>();

    constexpr const char* CERT_BEG = "-----BEGIN CERTIFICATE-----";
    constexpr const char* CERT_END = "-----END CERTIFICATE-----";
    auto c0 = combinedPem.find(CERT_BEG);
    auto c1 = combinedPem.find(CERT_END, c0);

    if (c0==std::string::npos || c1==std::string::npos)
      throw std::runtime_error("Certificate PEM block not found");

    c1 += strlen(CERT_END);
    const std::string serverCertPem = combinedPem.substr(c0, c1-c0);

    constexpr const char* KEY_BEG = "-----BEGIN RSA PRIVATE KEY-----";
    constexpr const char* KEY_END = "-----END RSA PRIVATE KEY-----";
    auto k0 = combinedPem.find(KEY_BEG);
    auto k1 = combinedPem.find(KEY_END, k0);
    if (k0==std::string::npos || k1==std::string::npos)
      throw std::runtime_error("Private-key PEM block not found");
    k1 += strlen(KEY_END);
    const std::string privateKeyPem = combinedPem.substr(k0, k1-k0);

    mbedtls_x509_crt serverCert;
    mbedtls_x509_crt_init(&serverCert);
    if (mbedtls_x509_crt_parse(&serverCert,
          (const unsigned char*)serverCertPem.c_str(),
          serverCertPem.size()+1) != 0)
      throw std::runtime_error("Failed to parse server certificate");
    updateLabel(0);


    const std::string caPem = doc["caCertificate"].as<std::string>();
    mbedtls_x509_crt caCert;
    mbedtls_x509_crt_init(&caCert);
    if (mbedtls_x509_crt_parse(&caCert,
          (const unsigned char*)caPem.c_str(),
          caPem.size()+1) != 0)
      throw std::runtime_error("Failed to parse CA certificate");
    updateLabel(1);

    const std::string gmkStr = doc["gmk"].as<std::string>();
    crypto::Key256 GMK = crypto::CryptoModule::strToKey256(gmkStr);

    updateLabel(2);

    std::vector<float> freqs;
    for (auto v : doc["frequencies"].as<JsonArray>())
      freqs.push_back(v.as<float>());

    updateLabel(3);

    std::vector<SoldierInfo> soldierInfos;
    for (auto v : doc["soldiers"].as<JsonArray>()) {
        const std::string pem = v.as<std::string>();

        SoldierInfo info;

        mbedtls_x509_crt_init(&info.cert);
        if (mbedtls_x509_crt_parse(&info.cert,
                (const unsigned char*)pem.c_str(),
                pem.size()+1) != 0) {
            mbedtls_x509_crt_free(&info.cert);
            continue;
        }

        char cn[128] = {0};
        for (auto p = &info.cert.subject; p; p = p->next) {
            if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &p->oid) == 0) {
            size_t len = std::min<size_t>(p->val.len, sizeof(cn)-1);
            memcpy(cn, p->val.p, len);
            cn[len] = '\0';
            break;
            }
        }
        info.name = cn;
        soldierInfos.push_back(std::move(info));
    }
    updateLabel(4);

    // 10) pull out the heart-beat interval
    const uint16_t intervalMs = doc["intervalMs"].as<uint16_t>();
    updateLabel(5);

    // 11) parse the private key
    mbedtls_pk_context privKey;
    mbedtls_pk_init(&privKey);
    if (mbedtls_pk_parse_key(&privKey,
          (const unsigned char*)privateKeyPem.c_str(),
          privateKeyPem.size()+1, nullptr, 0) != 0)
      throw std::runtime_error("Failed to parse private key");
    updateLabel(6);

    // 12) build your Soldier object
    this->soldierModule = std::make_unique<Soldier>(
      /* name         */ doc["name"].as<std::string>(),
      /* publicCert   */ serverCert,
      /* privateKey   */ privKey,
      /* caPublicCert */ caCert,
      /* soldierNum   */ doc["soldierNumber"].as<uint16_t>(),
      /* intervalMS   */ intervalMs
    );
    this->soldierModule->appendFrequencies(freqs);
    for (auto &si : soldierInfos)
      this->soldierModule->addCert(si.cert);

    // 13) clean up the locals (the Soldier constructor and addCert() should
    //     have taken ownership or copied as needed)
    mbedtls_x509_crt_free(&serverCert);
    mbedtls_x509_crt_free(&caCert);
    mbedtls_pk_free(&privKey);

  } 
  catch (const std::exception &e) {
    Serial.printf("⚠️ onSocketOpened error: %s\n", e.what());
  }
}

void receiveParametersPageSoldier::updateLabel(uint8_t index)
{
    lv_label_set_text(this->statusLabels[index], this->messages[index]);
}