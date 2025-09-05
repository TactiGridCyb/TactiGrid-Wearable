/* test.ino
   Demonstrates:
   1) Extracting the PEM‐encoded public key from a stored x509 certificate
   2) Emitting your own certificate as PEM
*/

#include <Arduino.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/pem.h>
#include <cstring>
#include <unordered_map>

// ——————————————————————————————
// Replace this with your actual certificate PEM
// ——————————————————————————————
const char testCertificate[] = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAK1eI+fY0e1YMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
BAYTAklOMQswCQYDVQQIDAJUUzELMAkGA1UEBwwCUlQxDTALBgNVBAoMBE15Q28x
DTALBgNVBAMMBE15Q0EwHhcNMjUwNjEwMTUyNzA3WhcNMjYwNjA5MTUyNzA3WjBF
MQswCQYDVQQGEwJJTjELMAkGA1UECAwCVFMxCzAJBgNVBAcMAlJUMQ0wCwYDVQQK
DARNeUNvMQ0wCwYDVQQDDARNeUNBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
CgKCAQEArX1Y6QOZnS/wR6e+uYb+Zz2YwN9Mcc0zB5X8o6kCxZV+BRIP8n5pxWpw
vNVyYHfkq0BXf1XKqA3v6eh5+qURxXfMZU+9Xg6f+2e8jwOwV+Uq+lkIyE3X1d56
n+NLu3tI3hp3WYcbIJvUXeK3+hCgtnk6XqbDdIoaADV2u0xqSZYHPRq8YizEqh/+
2Isqk9G0w2anjLtytNd+bsuQ2p5Zl8a1z5N6n5m+Xo3q5pqZ2u9PDMkDkdV6Roqp
P7w+n1u5ZMfkoH8tT5dR4Qh8VQQ7RkR4uGKTw0+PsGg6QpJtTp9xXBu2xHgXvFUM
siTVZJFQ6G8lw+f+6JQa7unM1L2bfwIDAQABo1AwTjAdBgNVHQ4EFgQUu+6yjL+C
j0OyxLZ9rDNkR5N2eF8wHwYDVR0jBBgwFoAUu+6yjL+Cj0OyxLZ9rDNkR5N2eF8w
DAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAGlfFp6KaF+/DE6YXQRho
J5p4Siy54G7I+z/HPIRkLkQ/OyWZrU44/A5jhFsJ+o3zHGfFXk8aeXQwEj2aD3qI
iI4CpQfT+Ws2VV3IxRB8PxiQW8W8z7GQfiF+mtOQhb6zUpQVoiqQYFL4vYp4kLL1
pRZjmA1Nl2IY8czcMbTJjB/5Wf6X8U8n54c7hB2Hl5nWa+YTB/0PpExf6G/5xG2n
2u1Rv95G9b6blgMOCh4M/6m5dE9Bj1T0RM3sZ2FwVjg5muTldswl7zv5J2r1FxQO
/GNgWom3QXLeQJbNHZc/DxAazRuigZ1hPVoa5RNJD+drb5PkdzvZD7eJ0uQ1RkM5
4w==
-----END CERTIFICATE-----
)CERT";

// ——————————————————————————————
// Data structures to mimic your handler
// ——————————————————————————————
struct CommanderInfo {
  mbedtls_x509_crt cert;
};

class Soldier {
public:
  std::unordered_map<uint8_t, CommanderInfo> commanders;
  mbedtls_x509_crt ownCert;

  mbedtls_x509_crt& getPublicCert() {
    return ownCert;
  }
};

Soldier soldier;

// ——————————————————————————————
// Your two functions under test
// ——————————————————————————————

String getCommanderPubKey(int commanderId) {
  // 1) grab the parsed certificate for this commander
  auto& crt = soldier.commanders
    .at(static_cast<uint8_t>(commanderId))
    .cert;

  // 2) point at its public-key context
  mbedtls_pk_context* pk = &crt.pk;

  // 3) emit PEM
  constexpr size_t PUBKEY_BUF_SIZE = 1600;
  unsigned char buf[PUBKEY_BUF_SIZE];
  int ret = mbedtls_pk_write_pubkey_pem(pk, buf, PUBKEY_BUF_SIZE);
  if (ret != 0) {
    Serial.printf("Error writing pubkey PEM: -0x%04X\n", -ret);
    return String();
  }
  return String(reinterpret_cast<char*>(buf));
}

String getCertificatePEM() {
  // 1) grab your own certificate
  auto& cert = soldier.getPublicCert();

  // 2) wrap DER → PEM
  constexpr size_t CERT_PEM_BUF_SIZE = 2048;
  unsigned char buf[CERT_PEM_BUF_SIZE];
  size_t olen = 0;
  int ret = mbedtls_pem_write_buffer(
    "-----BEGIN CERTIFICATE-----\n",
    "-----END CERTIFICATE-----\n",
    cert.raw.p,
    cert.raw.len,
    buf,
    CERT_PEM_BUF_SIZE,
    &olen
  );
  if (ret != 0) {
    Serial.printf("Error writing cert PEM: -0x%04X\n", -ret);
    return String();
  }
  return String(reinterpret_cast<char*>(buf));
}

// ——————————————————————————————
// Test harness
// ——————————————————————————————
void setup() {
  Serial.begin(115200);
  delay(5000);
  while (!Serial) { delay(10); }

  // 1) Parse testCertificate into commander #1
  mbedtls_x509_crt_init(&soldier.commanders[1].cert);
  if(mbedtls_x509_crt_parse(
       &soldier.commanders[1].cert,
       (const unsigned char*)testCertificate,
       strlen(testCertificate) + 1
     ) != 0) {
    Serial.println("❌ Failed to parse commander certificate");
    return;
  }

  // 2) Parse testCertificate as your own certificate
  mbedtls_x509_crt_init(&soldier.ownCert);
  if(mbedtls_x509_crt_parse(
       &soldier.ownCert,
       (const unsigned char*)testCertificate,
       strlen(testCertificate) + 1
     ) != 0) {
    Serial.println("❌ Failed to parse own certificate");
    return;
  }

  Serial.println("✅ Certificates parsed successfully\n");

  // 3) Test getCommanderPubKey
  Serial.println("==== Commander #1 Public Key PEM ====");
  Serial.println(getCommanderPubKey(1));

  // 4) Test getCertificatePEM
  Serial.println("==== Own Certificate PEM ====");
  Serial.println(getCertificatePEM());
}

void loop() {
  // nothing to do here
}
