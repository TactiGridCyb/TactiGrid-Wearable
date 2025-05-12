#include <Arduino.h>
#include "ECDHHelper.h"
#include "mbedtls/base64.h"

String toBase64(const std::vector<uint8_t>& data) {
  size_t len = 4 * ((data.size() + 2) / 3);
  std::vector<uint8_t> encoded(len);
  size_t olen;
  mbedtls_base64_encode(encoded.data(), encoded.size(), &olen, data.data(), data.size());
  return String((const char*)encoded.data());
}

void printHex(const char* label, const std::vector<uint8_t>& data) {
  Serial.print(label);
  for (uint8_t b : data) {
    Serial.printf("%02X", b);
  }
  Serial.println();
}

void testECDH() {
  Serial.println("🔐 Starting ECDH test...");

  ECDHHelper alice;
  ECDHHelper bob;

  if (!alice.generateEphemeralKey() || !bob.generateEphemeralKey()) {
    Serial.println("❌ Failed to generate keys");
    return;
  }

  std::vector<uint8_t> alicePub = alice.getPublicKeyRaw();
  std::vector<uint8_t> bobPub   = bob.getPublicKeyRaw();

  Serial.println("🔑 Alice Public Key (base64):");
  Serial.println(toBase64(alicePub));
  Serial.println("🔑 Bob Public Key (base64):");
  Serial.println(toBase64(bobPub));

  if (!alice.importPeerPublicKey(bobPub) || !bob.importPeerPublicKey(alicePub)) {
    Serial.println("❌ Failed to import peer key");
    return;
  }

  std::vector<uint8_t> secretA, secretB;
  if (!alice.deriveSharedSecret(secretA) || !bob.deriveSharedSecret(secretB)) {
    Serial.println("❌ Failed to derive shared secret");
    return;
  }

  Serial.println("🔐 Shared Secret (Alice):");
  printHex("A: ", secretA);
  Serial.println("🔐 Shared Secret (Bob):");
  printHex("B: ", secretB);

  if (secretA == secretB) {
    Serial.println("✅ Shared secret MATCH ✅");
  } else {
    Serial.println("❌ Shared secret MISMATCH ❌");
  }
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("✅ test start");
  testECDH();
}

void loop() {
  // no UI needed
}
