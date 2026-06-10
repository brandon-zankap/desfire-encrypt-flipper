#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DESFIRE_KEY_LEN 16

typedef struct {
    uint32_t app_id;        // 3-byte AID (e.g. 0x696969)
    uint8_t  file_id;       // 0-16
    uint8_t  key_index;     // 0-11
    uint8_t  app_key[DESFIRE_KEY_LEN]; // 16-byte AES key
    uint32_t start_cid;     // first CID to write
    uint32_t end_cid;       // last CID to write
    bool     change_key;    // whether to change key from default to app_key
    bool     decimal_cid;   // treat CID range as decimal (69 → 0x69 instead of 0x45)
} DesfireSeqConfig;

void desfire_seq_config_defaults(DesfireSeqConfig* cfg);
bool desfire_seq_config_load(DesfireSeqConfig* cfg);
bool desfire_seq_config_save(const DesfireSeqConfig* cfg);
