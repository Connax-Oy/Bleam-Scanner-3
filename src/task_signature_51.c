/** @file task_signature_51.c
 *
 * @defgroup task_signature Task Signature
 * @brief Signature generation and verification.
 * @{
 * @ingroup blesc_tasks
 * @}
 * @details More about that at @link_wiki_security.
 */
 
#include "task_signature.h"
#include "blesc_error.h"
#include "sdk_common.h"
#include "app_timer.h"
#include "log.h"
#include "nrf_drv_rng.h"
#include "task_connect_common.h"
#include "task_board.h"

#define NRF_CRYPTO_HASH_SIZE_SHA256 32                     /**< Size of SHA256 hash */
#define BLESC_BAD_SIGNATURE         NRF_ERROR_INVALID_DATA /**< Error code for bad signature */

__ALIGN(4) static uint8_t m_blesc_public_key[BLESC_PUBLIC_KEY_SIZE]; /**< Bleam Scanner public key copy for signature module. */

/**
 * @addtogroup task_signature
 * @{
 */

/**@brief Function for filling nRF51's @link_12_nrf_crypto_key_t structure with data.
 *
 * @param[out] key        Pointer to nrf_crypto_key_t structure.
 * @param[in]  data       Pointer to raw data array.
 * @param[in]  len        Length of raw data in bytes.
 *
 * @returns Nothing.
 */
static void convert_raw_to_nrf_crypto_key_sdk_12_3(nrf_crypto_key_t * key, uint8_t * data, size_t len) {
    key->p_le_data = data;
    key->len = len;
}

/**@brief Implementation of swap function for uint8_t.
 * @details This function is a helper function for @ref reverse_array
 *
 * @param[in,out] a        Pointer to the first instance.
 * @param[in,out] b        Pointer to the second instance.
 *
 * @returns Nothing.
 */
static void swap(uint8_t * a, uint8_t * b) {
    *a = *a ^ *b;
    *b = *a ^ *b;
    *a = *a ^ *b;
}

/**@brief Implementation of reverse array function.
 * @details This function is a helper function for @ref reverse_array_in_32_byte_chunks
 *
 * @param[in,out] array    Pointer to array
 * @param[in]     size     Size of array
 *
 * @returns Nothing.
 */
static void reverse_array(uint8_t * array, size_t size) {
    for(size_t index = 0; (size >> 1) > index; ++index) {
        swap(array + index, array + (size - 1) - index);
    }
}

/** @} end of task_signature */

void reverse_array_in_32_byte_chunks(uint8_t * array, size_t size) {
    for(size_t cnt = 0; cnt < size; cnt += 32) {
        reverse_array(array + cnt, (size - cnt < 32) ? size - cnt : 32);
    }
}

void create_blesc_public_key(blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code = NRF_SUCCESS;

    nrf_crypto_key_t internal_private_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_private_key,
                                           p_blesc_keys->blesc_private_key,
                                           BLESC_PRIVATE_KEY_SIZE);

    nrf_crypto_key_t internal_public_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_public_key,
                                           m_blesc_public_key,
                                           BLESC_PUBLIC_KEY_SIZE);

    err_code = nrf_crypto_public_key_compute(NRF_CRYPTO_CURVE_SECP256R1,
                                             &internal_private_key,
                                             &internal_public_key);
    APP_ERROR_CHECK(err_code);

    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Public key", m_blesc_public_key, BLESC_PUBLIC_KEY_SIZE);
}

void generate_blesc_keys(uint8_t * p_blesc_private_key, uint8_t * p_blesc_public_key) {
    ret_code_t err_code = NRF_SUCCESS;

    // Generate private and public keys for Bleam Scanner node

    err_code = nrf_drv_rng_rand(p_blesc_private_key, BLESC_PRIVATE_KEY_SIZE);
    APP_ERROR_CHECK(err_code);

    nrf_crypto_key_t internal_private_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_private_key,
                                           p_blesc_private_key,
                                           BLESC_PRIVATE_KEY_SIZE);
    nrf_crypto_key_t internal_public_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_public_key,
                                           p_blesc_public_key,
                                           BLESC_PUBLIC_KEY_SIZE);

    err_code = nrf_crypto_public_key_compute(NRF_CRYPTO_CURVE_SECP256R1,
                                             &internal_private_key,
                                             &internal_public_key);
    APP_ERROR_CHECK(err_code);
    // reverse keys and signatures and all
    reverse_array_in_32_byte_chunks(p_blesc_public_key, BLESC_PUBLIC_KEY_SIZE);

    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Private", p_blesc_private_key, BLESC_PRIVATE_KEY_SIZE);
    __LOG_XB(LOG_SRC_APP, LOG_LEVEL_INFO, "Public",  p_blesc_public_key,  BLESC_PUBLIC_KEY_SIZE);
}

void sign_data(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code;

    uint8_t hash[NRF_CRYPTO_HASH_SIZE_SHA256];
    nrf_crypto_key_t hashed_data;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&hashed_data,
                                           hash,
                                           NRF_CRYPTO_HASH_SIZE_SHA256);
    err_code = nrf_crypto_hash_compute(NRF_CRYPTO_HASH_ALG_SHA256,
                                       data,
                                       SALT_SIZE,
                                       &hashed_data);
    APP_ERROR_CHECK(err_code);

    nrf_crypto_key_t internal_private_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_private_key,
                                           p_blesc_keys->blesc_private_key,
                                           BLESC_PRIVATE_KEY_SIZE);

    nrf_crypto_key_t signature;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&signature,
                                           p_digest,
                                           BLESC_SIGNATURE_SIZE);
    err_code = nrf_crypto_sign(NRF_CRYPTO_CURVE_SECP256R1,
                               &internal_private_key,
                               &hashed_data,
                               &signature);
    APP_ERROR_CHECK(err_code);
  #ifdef BLESC_DEBUG_VERIFY_GENERATED_SIGNATURE
    wdt_feed();
    nrf_crypto_key_t internal_public_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_public_key,
                                           m_blesc_public_key,
                                           BLESC_PUBLIC_KEY_SIZE);
    err_code = nrf_crypto_verify(NRF_CRYPTO_CURVE_SECP256R1,
                                 &internal_public_key,
                                 &hashed_data,
                                 &signature);
    ASSERT(NRF_SUCCESS == err_code);
  #endif

    reverse_array_in_32_byte_chunks(p_digest, BLESC_SIGNATURE_SIZE);
}

bool sign_verify(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys) {
    ret_code_t err_code;

    __ALIGN(4) uint8_t hash[NRF_CRYPTO_HASH_SIZE_SHA256];
    static nrf_crypto_key_t hashed_data;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&hashed_data,
                                           hash,
                                           NRF_CRYPTO_HASH_SIZE_SHA256);
    err_code = nrf_crypto_hash_compute(NRF_CRYPTO_HASH_ALG_SHA256,
                                       data,
                                       SALT_SIZE,
                                       &hashed_data);
    APP_ERROR_CHECK(err_code);

    reverse_array_in_32_byte_chunks(p_digest, BLESC_SIGNATURE_SIZE);

    static nrf_crypto_key_t internal_public_key;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&internal_public_key,
                                           p_blesc_keys->bleam_public_key,
                                           BLESC_PUBLIC_KEY_SIZE);
    nrf_crypto_key_t signature;
    convert_raw_to_nrf_crypto_key_sdk_12_3(&signature,
                                           p_digest,
                                           BLESC_SIGNATURE_SIZE);
    wdt_feed();
    err_code = nrf_crypto_verify(NRF_CRYPTO_CURVE_SECP256R1,
                                 &internal_public_key,
                                 &hashed_data,
                                 &signature);

    if (err_code == NRF_SUCCESS) {
        return true;
    } else if (err_code == BLESC_BAD_SIGNATURE) {
        return false;
    } else {
        APP_ERROR_CHECK(err_code);
        return false;
    }
}
