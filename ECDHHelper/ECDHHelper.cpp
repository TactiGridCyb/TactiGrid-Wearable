#include "ECDHHelper.h"
#include "mbedtls/error.h"
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

bool ECDHHelper::importPeerPublicKey(const std::vector<uint8_t>& peerKey) {
    return mbedtls_ecp_point_read_binary(&ctx.grp, &ctx.Qp, peerKey.data(), peerKey.size()) == 0;
}

bool ECDHHelper::deriveSharedSecret(std::vector<uint8_t>& out) {
    out.resize(32);  // 256 bits
    size_t slen = 0;

    int ret = mbedtls_ecdh_calc_secret(&ctx, &slen, out.data(), out.size(),
                                       mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) return false;

    out.resize(slen);
    return true;
}
