#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t rk[44]; // round keys for AES-128 (11 rounds * 4 words)
} Aes128Context;

void aes128_set_key(Aes128Context* ctx, const uint8_t key[16]);
void aes128_encrypt_block(const Aes128Context* ctx, const uint8_t in[16], uint8_t out[16]);
void aes128_decrypt_block(const Aes128Context* ctx, const uint8_t in[16], uint8_t out[16]);

void aes128_cbc_encrypt(
    const Aes128Context* ctx,
    uint8_t iv[16],
    const uint8_t* in,
    uint8_t* out,
    size_t len);

void aes128_cbc_decrypt(
    const Aes128Context* ctx,
    uint8_t iv[16],
    const uint8_t* in,
    uint8_t* out,
    size_t len);
