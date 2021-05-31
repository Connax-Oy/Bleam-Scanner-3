#include "task_signature.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"

#define BLESC_BAD_SIGNATURE NRF_ERROR_CRYPTO_ECDSA_INVALID_SIGNATURE /**< Error code for bad signature */

__ALIGN(4) static uint8_t m_blesc_public_key[BLESC_PUBLIC_KEY_SIZE]; /**< Bleam Scanner public key copy for signature module. */

void create_blesc_public_key(blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code = NRF_SUCCESS;

    static nrf_crypto_ecc_private_key_t internal_private_key;
    err_code = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                   &internal_private_key,
                                                   p_blesc_keys->blesc_private_key,
                                                   BLESC_PRIVATE_KEY_SIZE);
    APP_ERROR_CHECK(err_code);
    static nrf_crypto_ecc_public_key_t internal_public_key;
    nrf_crypto_ecc_public_key_calculate_context_t keygen_ctx;
    err_code = nrf_crypto_ecc_public_key_calculate(&keygen_ctx,
                                                   &internal_private_key,
                                                   &internal_public_key);

    size_t key_size = BLESC_PUBLIC_KEY_SIZE;
    err_code = nrf_crypto_ecc_public_key_to_raw(&internal_public_key,
                                                m_blesc_public_key,
                                                &key_size);
    APP_ERROR_CHECK(err_code);

    nrf_crypto_ecc_private_key_free(&internal_private_key);
    nrf_crypto_ecc_public_key_free(&internal_public_key);

    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Public key", m_blesc_public_key, BLESC_PUBLIC_KEY_SIZE);
}

void generate_blesc_keys(uint8_t * p_blesc_private_key, uint8_t * p_blesc_public_key) {
    ret_code_t err_code = NRF_SUCCESS;

    // Generate private and public keys for Bleam Scanner node
    nrf_crypto_ecc_key_pair_generate_context_t keygen_ctx;
    nrf_crypto_ecc_private_key_t internal_private_key;
    nrf_crypto_ecc_public_key_t internal_public_key;
    err_code = nrf_crypto_ecc_key_pair_generate(&keygen_ctx,
                                                &g_nrf_crypto_ecc_secp256r1_curve_info,
                                                &internal_private_key,
                                                &internal_public_key);
    APP_ERROR_CHECK(err_code);
    size_t key_size;

    key_size = BLESC_PRIVATE_KEY_SIZE;
    err_code = nrf_crypto_ecc_private_key_to_raw(&internal_private_key,
                                                 p_blesc_private_key,
                                                 &key_size);
    APP_ERROR_CHECK(err_code);
    nrf_crypto_ecc_private_key_free(&internal_private_key);
    key_size = BLESC_PUBLIC_KEY_SIZE;
    err_code = nrf_crypto_ecc_public_key_to_raw(&internal_public_key,
                                                p_blesc_public_key,
                                                &key_size);
    APP_ERROR_CHECK(err_code);
    nrf_crypto_ecc_public_key_free(&internal_public_key);

    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Private", p_blesc_private_key, BLESC_PRIVATE_KEY_SIZE);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Public",  p_blesc_public_key,  BLESC_PUBLIC_KEY_SIZE);
}

void sign_data(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code;

    uint8_t hashed_data[NRF_CRYPTO_HASH_SIZE_SHA256];
    size_t hash_size = NRF_CRYPTO_HASH_SIZE_SHA256;
    nrf_crypto_backend_hash_context_t hash_ctx;
    err_code = nrf_crypto_hash_calculate(&hash_ctx,
                                         &g_nrf_crypto_hash_sha256_info,
                                         data,
                                         SALT_SIZE,
                                         hashed_data,
                                         &hash_size);
    APP_ERROR_CHECK(err_code);

    static nrf_crypto_ecc_private_key_t internal_private_key;
    err_code = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                   &internal_private_key,
                                                   p_blesc_keys->blesc_private_key,
                                                   BLESC_PRIVATE_KEY_SIZE);
    APP_ERROR_CHECK(err_code);

    size_t signature_size = BLESC_SIGNATURE_SIZE;
    err_code = nrf_crypto_ecdsa_sign(NULL,
                                     &internal_private_key,
                                     hashed_data,
                                     NRF_CRYPTO_HASH_SIZE_SHA256,
                                     p_digest,
                                     &signature_size);
    APP_ERROR_CHECK(err_code);

    // Verify signature correctness
    static nrf_crypto_ecc_public_key_t internal_public_key;
    err_code = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                  &internal_public_key,
                                                  m_blesc_public_key,
                                                  BLESC_PUBLIC_KEY_SIZE);
  #ifdef BLESC_DEBUG_VERIFY_GENERATED_SIGNATURE
    err_code = nrf_crypto_ecdsa_verify(NULL,
                                       &internal_public_key,
                                       hashed_data,
                                       NRF_CRYPTO_HASH_SIZE_SHA256,
                                       p_digest,
                                       BLESC_SIGNATURE_SIZE);
    // Key deallocation
    nrf_crypto_ecc_public_key_free(&internal_public_key);
    nrf_crypto_ecc_private_key_free(&internal_private_key);
    ASSERT(NRF_SUCCESS == err_code);
  #endif
}

bool sign_verify(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code;

    uint8_t hashed_data[NRF_CRYPTO_HASH_SIZE_SHA256];
    nrf_crypto_backend_hash_context_t hash_ctx;
    size_t hash_size = NRF_CRYPTO_HASH_SIZE_SHA256;
    err_code = nrf_crypto_hash_calculate(&hash_ctx,
                                         &g_nrf_crypto_hash_sha256_info,
                                         data,
                                         SALT_SIZE,
                                         hashed_data,
                                         &hash_size);
    APP_ERROR_CHECK(err_code);

    static nrf_crypto_ecc_public_key_t internal_public_key;
    err_code = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                  &internal_public_key,
                                                  p_blesc_keys->bleam_public_key,
                                                  BLESC_PUBLIC_KEY_SIZE);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_crypto_ecdsa_verify(NULL,
                                       &internal_public_key,
                                       hashed_data,
                                       NRF_CRYPTO_HASH_SIZE_SHA256,
                                       p_digest,
                                       BLESC_SIGNATURE_SIZE);
    nrf_crypto_ecc_public_key_free(&internal_public_key);

    if (err_code == NRF_SUCCESS) {
        return true;
    } else if (err_code == BLESC_BAD_SIGNATURE) {
        return false;
    } else {
        APP_ERROR_CHECK(err_code);
        return false;
    }
}
