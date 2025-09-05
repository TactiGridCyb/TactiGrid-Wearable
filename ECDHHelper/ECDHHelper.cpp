#include "ECDHHelper.h"
#include "mbedtls/error.h"
#include <Arduino.h>   // <-- add this
#include <cstring>

ECDHHelper::ECDHHelper() {
    mbedtls_ecdh_init(&ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
}


ECDHHelper::~ECDHHelper() {
    mbedtls_ecdh_free(&ctx);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

bool ECDHHelper::generateEphemeralKey() {
    int ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) return false;

    ret = mbedtls_ecdh_gen_public(&ctx.grp, &ctx.d, &ctx.Q, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) return false;

    keyGenerated = true;
    return true;
}

std::vector<uint8_t> ECDHHelper::getPublicKeyRaw() const {
    std::vector<uint8_t> buf(100);  // Over-allocate
    size_t olen = 0;
    mbedtls_ecp_point_write_binary(&ctx.grp, &ctx.Q,
        MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf.data(), buf.size());

    buf.resize(olen);
    return buf;
}

bool ECDHHelper::importPeerPublicKey(const std::vector<uint8_t>& buf) {
    int ret;

    // ————— 0) load the P-256 curve into ctx.grp —————
    ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ import: group_load failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    Serial.printf("[ECDH] grp.id = %d, buf.len = %u\n", ctx.grp.id, (unsigned)buf.size());

    // ————— 1) read the raw (0x04||X||Y) into Qp —————
    ret = mbedtls_ecp_point_read_binary(&ctx.grp, &ctx.Qp,
                                        buf.data(), buf.size());
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ import: read_binary failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    // ————— 2) sanity‐check that Qp is on the curve —————
    ret = mbedtls_ecp_check_pubkey(&ctx.grp, &ctx.Qp);
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ import: pubkey check failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    Serial.println("✅ importPeerPublicKey OK");
    return true;
}




bool ECDHHelper::deriveSharedSecret(std::vector<uint8_t>& out) {
    // For a 256-bit curve, secret will be 32 bytes.
    size_t max_len = (ctx.grp.pbits + 7) / 8;
    out.resize(max_len);

    size_t slen = 0;
    Serial.println("  [ECDH] calling mbedtls_ecdh_calc_secret…");
    int ret = mbedtls_ecdh_calc_secret(&ctx, &slen,
                                       out.data(), out.size(),
                                       mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("  ❌ derive: mbedtls_ecdh_calc_secret failed: -0x%04X (%s)\n",
                      -ret, err);
        return false;
    }

    // now shrink to the real length
    out.resize(slen);
    Serial.printf("  ✅ deriveSharedSecret OK, slen=%u\n", (unsigned)slen);
    return true;
}