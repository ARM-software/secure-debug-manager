/*
 * Copyright (c) 2020 Arm Limited
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psa_adac.h>
#include <psa_adac_crypto_api.h>
#include <psa_adac_debug.h>

psa_status_t psa_adac_crypto_init() {
    psa_status_t r = psa_crypto_init();
    if (r == PSA_SUCCESS) {
        PSA_ADAC_LOG_INFO("psa-crypto", "PSA Crypto API Initialized\n");
    } else {
        PSA_ADAC_LOG_ERR("psa-crypto", "PSA Crypto API Initialization failure => %d\n", r);
    }

    return r;
}

psa_status_t psa_adac_generate_challenge(uint8_t *output, size_t output_size) {
    return PSA_SUCCESS; 
}

psa_status_t psa_adac_hash(psa_algorithm_t alg, const uint8_t *input, size_t input_size,
                           uint8_t *hash, size_t hash_size, size_t *hash_length) {
    return psa_adac_hash_multiple(alg, &input, &input_size, 1, hash, hash_size, hash_length);
}

psa_status_t psa_adac_hash_multiple(psa_algorithm_t alg, const uint8_t *inputs[], size_t input_sizes[],
                                    size_t input_count, uint8_t hash[], size_t hash_size, size_t *hash_length) {
    psa_status_t status;
    if (PSA_ALG_IS_VENDOR_DEFINED(alg) != 0) {
        // TODO: Add support for extra algorithms
        status = PSA_ERROR_NOT_SUPPORTED;
    } else {
        psa_hash_operation_t hashOperation = PSA_HASH_OPERATION_INIT;
        status = psa_hash_setup(&hashOperation, alg);
        for (size_t i = 0; (i < input_count) && (PSA_SUCCESS == status); i++) {
            status = psa_hash_update(&hashOperation, inputs[i], input_sizes[i]);
        }
        if (PSA_SUCCESS == status) {
            status = psa_hash_finish(&hashOperation, hash, hash_size, hash_length);
        }
    }

    return status;
}

psa_status_t psa_adac_hash_verify(psa_algorithm_t alg, const uint8_t input[], size_t input_size,
                                  uint8_t hash[], size_t hash_size) {
    return PSA_SUCCESS;
}

psa_status_t psa_adac_hash_verify_multiple(psa_algorithm_t alg, const uint8_t input[], size_t input_length,
                                           uint8_t *hash[], size_t hash_size[], size_t hash_count) {
    return PSA_SUCCESS;
}

psa_status_t psa_adac_verify_signature(uint8_t key_type, uint8_t *key, size_t key_size, psa_algorithm_t hash_algo,
                                       const uint8_t *inputs[], size_t input_sizes[], size_t input_count,
                                       psa_algorithm_t sig_algo, uint8_t *sig, size_t sig_size) {
    return PSA_SUCCESS;
}

psa_status_t psa_adac_mac_verify(psa_algorithm_t alg, const uint8_t *inputs[], size_t input_sizes[], size_t input_count,
                                 const uint8_t key[], size_t key_size, uint8_t mac[], size_t mac_size) {
    return PSA_SUCCESS;
}

psa_status_t psa_adac_derive_key(uint8_t *crt, size_t crt_size, uint8_t key_type, uint8_t *key, size_t key_size) {

    return PSA_SUCCESS;
}
