/**
 * @addtogroup task_signature
 * @{
 */
#ifndef BLESC_SIGNATURE_H__
#define BLESC_SIGNATURE_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "app_util_platform.h"
#include "app_config.h"
#include "global_app_config.h"

#include "nrf_crypto.h"
#if defined(SDK_15_3)
  #include "nrf_crypto_ecc.h"
  #include "nrf_crypto_hash.h"
#endif
#if defined(SDK_12_3)
  #include "ecc.h"
  #include "sha256.h"
#endif

#define BLESC_PRIVATE_KEY_SIZE 32                           /**< Size of private key for SEC256R1 */
#define BLESC_PUBLIC_KEY_SIZE  64                           /**< Size of public keys for SEC256R1 */
#define BLESC_SIGNATURE_SIZE   64                           /**< Size of SEC256R1 signature */
#define SIGN_KEY_MIN_SIZE      2                            /**< Minimal size of key for signing. */
#define SIGN_KEY_MAX_SIZE      128                          /**< Maximal size of key for signing. */
#define HEX_MAX_BUF_SIZE       2 + (SIGN_KEY_MAX_SIZE << 1) /**< Maximal size of hex buffer for signing. */
#define SALT_SIZE              APP_CONFIG_DATA_CHUNK_SIZE   /**< Size of salt for signing. */

typedef struct {
    uint8_t  blesc_private_key[BLESC_PRIVATE_KEY_SIZE]; /**< Bleam Scanner node private key */
    uint8_t  bleam_public_key[BLESC_PUBLIC_KEY_SIZE];   /**< Bleam setup public key */
} blesc_keys_t;

/**@brief Function for saving Bleam Scanner public key copy to signature module.
 *
 * @param[out] p_blesc_keys   Pointer to Bleam Scanner keys structure.
 *
 * @returns Nothing.
 */
void create_blesc_public_key(blesc_keys_t * p_blesc_keys);

#ifdef SDK_12_3
/**@brief Function for converting array to nRF51 format.
 *
 * @details Keys in crypto library for nRF51 uses a different format,
 *          reversing each 32-byte chunk of data.
 *
 * @param[in,out] array    Pointer to array
 * @param[in]     size     Size of array
 *
 * @returns Nothing.
 */
void reverse_array_in_32_byte_chunks(uint8_t * array, size_t size);
#endif

/**@brief Function for creating a pair of SEC256R1 keys.
 *
 * @param[out] p_blesc_private_key   Pointer to array to store Bleam Scanner private key.
 * @param[out] p_blesc_public_key    Pointer to array to store Bleam Scanner public key.
 *
 * @returns Nothing.
 */
void generate_blesc_keys(uint8_t * p_blesc_private_key, uint8_t * p_blesc_public_key);

/**@brief Function for creating Bleam Scanner signature digest with SHA256 and secp256r1 curve.
 *
 * @param[out] p_digest              Pointer to array to store the signature in.
 * @param[in]  data                  Pointer to array with data to sign.
 * @param[in]  p_blesc_keys          Pointer to Bleam Scanner keys data struct.
 *
 * @returns Nothing.
 */
void sign_data(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys);

/**@brief Function for verifying Bleam signature digest with SHA256 and secp256r1 curve.
 *
 * @param[in]  p_digest              Pointer to array with signature.
 * @param[in]  data                  Pointer to array with signed data.
 * @param[in]  p_blesc_keys          Pointer to Bleam Scanner keys data struct.
 *
 * @returns Nothing.
 */
bool sign_verify(uint8_t *p_digest, uint8_t *data, blesc_keys_t * p_blesc_keys);

#endif // BLESC_SIGNATURE_H__

/** @}*/