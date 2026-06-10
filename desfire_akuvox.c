#include "desfire_akuvox.h"
#include <string.h>
#include <stdio.h>
#include <furi.h>
#include <furi_hal.h>
#include "aes128.h"

#define TAG "DesfireAkuvox"

#define DF_CMD_AUTHENTICATE_ISO  0xAA
#define DF_CMD_ADDITIONAL_FRAME  0xAF
#define DF_CMD_SELECT_APP        0x5A
#define DF_CMD_CREATE_APP        0xCA
#define DF_CMD_CREATE_STD_FILE   0xCD
#define DF_CMD_CHANGE_KEY        0xC4
#define DF_CMD_WRITE_DATA        0x3D
#define DF_CMD_READ_DATA         0xBD
#define DF_CMD_GET_FILE_SETTINGS 0xF5

#define DF_STATUS_OK             0x00
#define DF_STATUS_AF             0xAF
#define DF_STATUS_DUPLICATE      0xDE
#define DF_STATUS_AUTH_ERROR     0xAE
#define DF_STATUS_PERMISSION     0x9D
#define DF_STATUS_APP_NOT_FOUND  0xA0
#define DF_STATUS_FILE_NOT_FOUND 0xF0

static const uint8_t ZERO_KEY[16] = {0};

static bool df_native_cmd(
    NfcTransport* nfc,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t* data,
    size_t* data_len,
    uint8_t* status) {
    uint8_t rx[256];
    size_t rx_len = sizeof(rx);
    if(!nfc_transport_txrx(nfc, cmd, cmd_len, rx, &rx_len, 3000)) return false;
    if(rx_len < 1) return false;

    // Log the raw response for debugging - Elevated to INFO to ensure visibility in qFlipper
    char hex_log[128];
    int pos = 0;
    for(size_t i = 0; i < (rx_len > 16 ? 16 : rx_len); i++)
        pos += snprintf(hex_log + pos, sizeof(hex_log) - pos, "%02X ", rx[i]);
    FURI_LOG_I(TAG, "Rx(%zu): %s%s", rx_len, hex_log, rx_len > 16 ? "..." : "");

    // Revert to original parsing: status is the first byte for Native commands.
    *status = rx[0];
    size_t payload_len = rx_len - 1;
    if(data && data_len) {
        if(payload_len > *data_len) payload_len = *data_len;
        if(payload_len > 0) memcpy(data, rx + 1, payload_len);
        *data_len = payload_len;
    }

    return true;
}

static void aid_to_le(uint32_t aid, uint8_t out[3]) {
    out[0] = aid & 0xFF;
    out[1] = (aid >> 8) & 0xFF;
    out[2] = (aid >> 16) & 0xFF;
}

static void cid_to_be(uint32_t cid, uint8_t out[4]) {
    out[0] = (cid >> 24) & 0xFF;
    out[1] = (cid >> 16) & 0xFF;
    out[2] = (cid >> 8) & 0xFF;
    out[3] = cid & 0xFF;
}

static DesfireResult df_select_app(NfcTransport* nfc, uint32_t aid) {
    uint8_t cmd[4];
    cmd[0] = DF_CMD_SELECT_APP;
    aid_to_le(aid, &cmd[1]);
    uint8_t st;
    size_t len = 0;
    if(!df_native_cmd(nfc, cmd, sizeof(cmd), NULL, &len, &st))
        return DesfireResultTransport;
    if(st != DF_STATUS_OK) return DesfireResultSelectAppFailed;
    return DesfireResultOk;
}

static DesfireResult
    df_create_app(NfcTransport* nfc, uint32_t aid, uint8_t key_index) {
    uint8_t cmd[6];
    cmd[0] = DF_CMD_CREATE_APP;
    aid_to_le(aid, &cmd[1]);
    cmd[4] = 0x0B;
    uint8_t num_keys = key_index + 1;
    if(num_keys < 1) num_keys = 1;
    cmd[5] = 0x80 | num_keys;
    uint8_t st;
    size_t len = 0;
    if(!df_native_cmd(nfc, cmd, sizeof(cmd), NULL, &len, &st))
        return DesfireResultTransport;
    if(st == DF_STATUS_OK || st == DF_STATUS_DUPLICATE)
        return DesfireResultOk;
    FURI_LOG_E(TAG, "CreateApp status: %02X", st);
    return DesfireResultCreateAppFailed;
}

static DesfireResult df_create_file(
    NfcTransport* nfc,
    uint8_t file_id,
    uint8_t key_index) {
    uint8_t cmd[8];
    cmd[0] = DF_CMD_CREATE_STD_FILE;
    cmd[1] = file_id;
    cmd[2] = 0x03; // Encrypted communication
    uint8_t ki = key_index & 0x0F;
    cmd[3] = (ki << 4) | ki;
    cmd[4] = (ki << 4) | ki;
    cmd[5] = 0x08; // 8 bytes (matches Akuvox card)
    cmd[6] = 0x00;
    cmd[7] = 0x00;
    uint8_t st;
    size_t len = 0;
    if(!df_native_cmd(nfc, cmd, sizeof(cmd), NULL, &len, &st))
        return DesfireResultTransport;
    if(st == DF_STATUS_OK || st == DF_STATUS_DUPLICATE)
        return DesfireResultOk;
    FURI_LOG_E(TAG, "CreateFile status: %02X", st);
    return DesfireResultCreateFileFailed;
}

static void rotate_left(const uint8_t* in, uint8_t* out, size_t len) {
    for(size_t i = 0; i < len - 1; i++) out[i] = in[i + 1];
    out[len - 1] = in[0];
}

typedef struct {
    uint8_t key[16];
    uint8_t iv[16];
    bool    active;
} DesfireSession;

static DesfireSession session = {0};

static void df_session_reset(void) {
    memset(&session, 0, sizeof(session));
}

// AES-CMAC subkey generation (RFC 4493)
static void df_cmac_subkeys(uint8_t k1[16], uint8_t k2[16]) {
    Aes128Context aes;
    aes128_set_key(&aes, session.key);
    uint8_t zero[16] = {0};
    uint8_t L[16];
    aes128_encrypt_block(&aes, zero, L);

    // K1 = L << 1; if MSB(L)==1: K1 ^= Rb (Rb=0x87)
    uint8_t msb = L[0] & 0x80;
    for(int i = 0; i < 15; i++) k1[i] = (L[i] << 1) | (L[i + 1] >> 7);
    k1[15] = L[15] << 1;
    if(msb) k1[15] ^= 0x87;

    // K2 = K1 << 1; if MSB(K1)==1: K2 ^= Rb
    msb = k1[0] & 0x80;
    for(int i = 0; i < 15; i++) k2[i] = (k1[i] << 1) | (k1[i + 1] >> 7);
    k2[15] = k1[15] << 1;
    if(msb) k2[15] ^= 0x87;
}

// Compute AES-CMAC over msg and update session.iv
static void df_cmac(const uint8_t* msg, size_t msg_len) {
    if(!session.active) return;

    uint8_t k1[16], k2[16];
    df_cmac_subkeys(k1, k2);

    Aes128Context aes;
    aes128_set_key(&aes, session.key);

    size_t n = (msg_len + 15) / 16;
    if(n == 0) n = 1;
    bool complete = (msg_len > 0) && (msg_len % 16 == 0);

    uint8_t x[16];
    memcpy(x, session.iv, 16);

    // Process all full blocks except last
    for(size_t i = 0; i < n - 1; i++) {
        for(int j = 0; j < 16; j++) x[j] ^= msg[i * 16 + j];
        aes128_encrypt_block(&aes, x, x);
    }

    // Last block: pad if incomplete, XOR with K1 or K2
    uint8_t last[16];
    memset(last, 0, 16);
    size_t last_start = (n - 1) * 16;
    size_t remaining = msg_len - last_start;
    if(remaining > 0) memcpy(last, msg + last_start, remaining);

    if(complete) {
        for(int j = 0; j < 16; j++) last[j] ^= k1[j];
    } else {
        last[remaining] = 0x80;
        for(int j = 0; j < 16; j++) last[j] ^= k2[j];
    }

    for(int j = 0; j < 16; j++) x[j] ^= last[j];
    aes128_encrypt_block(&aes, x, x);

    memcpy(session.iv, x, 16);
}

// DESFire CRC32 for AES sessions (init=0xFFFFFFFF, poly=0xEDB88320, no final XOR)
static uint32_t df_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int j = 0; j < 8; j++) {
            if(crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc;
}

static void df_decrypt_session(const uint8_t* in, uint8_t* out, size_t len) {
    if(!session.active) {
        memcpy(out, in, len);
        return;
    }
    Aes128Context aes;
    aes128_set_key(&aes, session.key);
    
    // In AES-CBC decryption, the next IV is the last block of ciphertext
    uint8_t next_iv[16];
    memcpy(next_iv, in + len - 16, 16);
    
    aes128_cbc_decrypt(&aes, session.iv, in, out, len);
    
    memcpy(session.iv, next_iv, 16);
}

static DesfireResult df_authenticate_aes(
    NfcTransport* nfc,
    uint8_t key_index,
    const uint8_t key[16]) {
    df_session_reset();
    uint8_t cmd1[2] = {DF_CMD_AUTHENTICATE_ISO, key_index};
    uint8_t resp[64];
    size_t resp_len = sizeof(resp);
    uint8_t st;

    if(!df_native_cmd(nfc, cmd1, sizeof(cmd1), resp, &resp_len, &st))
        return DesfireResultTransport;
    if(st != DF_STATUS_AF || resp_len != 16) {
        FURI_LOG_E(TAG, "Auth step1: st=%02X len=%zu", st, resp_len);
        return DesfireResultAuthFailed;
    }

    uint8_t enc_rndb[16];
    memcpy(enc_rndb, resp, 16);

    Aes128Context aes;
    aes128_set_key(&aes, key);
    uint8_t iv[16];
    memset(iv, 0, 16);
    uint8_t rndb[16];
    aes128_cbc_decrypt(&aes, iv, enc_rndb, rndb, 16);

    uint8_t rndb_rot[16];
    rotate_left(rndb, rndb_rot, 16);

    uint8_t rnda[16];
    furi_hal_random_fill_buf(rnda, 16);

    uint8_t plain[32];
    memcpy(plain, rnda, 16);
    memcpy(plain + 16, rndb_rot, 16);

    memcpy(iv, enc_rndb, 16);
    uint8_t cipher[32];
    aes128_cbc_encrypt(&aes, iv, plain, cipher, 32);

    uint8_t cmd2[33];
    cmd2[0] = DF_CMD_ADDITIONAL_FRAME;
    memcpy(cmd2 + 1, cipher, 32);
    resp_len = sizeof(resp);

    if(!df_native_cmd(nfc, cmd2, sizeof(cmd2), resp, &resp_len, &st))
        return DesfireResultTransport;
    if(st != DF_STATUS_OK || resp_len != 16) {
        FURI_LOG_E(TAG, "Auth step2: st=%02X len=%zu", st, resp_len);
        return DesfireResultAuthFailed;
    }

    // Auth Success
    // Session Key: RndA[0..3] | RndB[0..3] | RndA[12..15] | RndB[12..15]
    memcpy(session.key, rnda, 4);
    memcpy(session.key + 4, rndb, 4);
    memcpy(session.key + 8, rnda + 12, 4);
    memcpy(session.key + 12, rndb + 12, 4);

    // Initial IV for ISO Auth (0xAA) is reset to ZERO for the first command
    memset(session.iv, 0, 16);
    session.active = true;

    FURI_LOG_I(TAG, "AES auth success with key index %u", key_index);
    return DesfireResultOk;
}

static DesfireResult df_change_key_simple(
    NfcTransport* nfc,
    uint8_t key_index,
    const uint8_t new_key[16]) {
    FURI_LOG_W(
        TAG,
        "ChangeKey: session-key crypto not yet implemented. "
        "Key must be pre-set via ACRM tool or will remain default.");
    (void)nfc;
    (void)key_index;
    (void)new_key;
    return DesfireResultOk;
}

// Encrypted write: CMAC over header, CRC32 over header+data, AES-CBC encrypt data+CRC+padding
static DesfireResult df_write_data_encrypted(
    NfcTransport* nfc,
    uint8_t file_id,
    const uint8_t* data,
    size_t len) {
    if(len > 16 || !session.active) return DesfireResultWriteFailed;

    // Command header (sent in plain)
    uint8_t hdr[8] = {
        DF_CMD_WRITE_DATA, file_id,
        0x00, 0x00, 0x00,
        (uint8_t)(len & 0xFF), 0x00, 0x00};

    // For encrypted writes: NO CMAC over header (unlike reads).
    // The CRC inside the encrypted block provides integrity.
    // IV stays at current value (0 after auth).

    // CRC32 over: header + plaintext data
    uint8_t crc_buf[24];
    memcpy(crc_buf, hdr, 8);
    memcpy(crc_buf + 8, data, len);
    uint32_t crc = df_crc32(crc_buf, 8 + len);

    // Plaintext = data || CRC32(LE) || zero-padding to 16-byte boundary
    size_t pad_len = len + 4; // data + CRC
    pad_len = (pad_len + 15) & ~(size_t)15; // round up to 16
    uint8_t plain[32];
    memset(plain, 0, sizeof(plain));
    memcpy(plain, data, len);
    plain[len + 0] = (uint8_t)(crc & 0xFF);
    plain[len + 1] = (uint8_t)((crc >> 8) & 0xFF);
    plain[len + 2] = (uint8_t)((crc >> 16) & 0xFF);
    plain[len + 3] = (uint8_t)((crc >> 24) & 0xFF);

    // Encrypt with AES-CBC (session.iv updated in-place)
    uint8_t enc[32];
    Aes128Context aes;
    aes128_set_key(&aes, session.key);
    aes128_cbc_encrypt(&aes, session.iv, plain, enc, pad_len);

    // Send: header(8) + ciphertext
    uint8_t full_cmd[40];
    memcpy(full_cmd, hdr, 8);
    memcpy(full_cmd + 8, enc, pad_len);

    uint8_t st;
    size_t rlen = 0;
    if(!df_native_cmd(nfc, full_cmd, 8 + pad_len, NULL, &rlen, &st))
        return DesfireResultTransport;
    if(st != DF_STATUS_OK) {
        FURI_LOG_E(TAG, "EncWriteData status: %02X", st);
        return DesfireResultWriteFailed;
    }
    return DesfireResultOk;
}

DesfireResult desfire_akuvox_write_cid(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t cid) {
    DesfireResult r;

    r = df_select_app(nfc, cfg->app_id);
    if(r != DesfireResultOk) return r;

    r = df_authenticate_aes(nfc, cfg->key_index, cfg->app_key);
    if(r == DesfireResultAuthFailed) {
        r = df_authenticate_aes(nfc, cfg->key_index, ZERO_KEY);
        if(r != DesfireResultOk) return DesfireResultAuthFailed;
    } else if(r != DesfireResultOk) {
        return r;
    }

    // After auth: IV = 0, session active
    uint8_t cid_bytes[4];
    cid_to_be(cid, cid_bytes);
    r = df_write_data_encrypted(nfc, cfg->file_id, cid_bytes, 4);
    if(r != DesfireResultOk) return r;

    // Verify by reading back (re-auths internally)
    uint32_t readback_cid = 0;
    r = desfire_akuvox_read_cid(nfc, cfg, &readback_cid);
    if(r != DesfireResultOk) return r;

    if(readback_cid != cid) {
        FURI_LOG_E(TAG, "Verify mismatch! wrote=%lu read=%lu", cid, readback_cid);
        return DesfireResultVerifyFailed;
    }

    FURI_LOG_I(TAG, "CID %lu written and verified", cid);
    return DesfireResultOk;
}

DesfireResult desfire_akuvox_personalise(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t cid) {
    DesfireResult r;

    r = df_select_app(nfc, 0x000000);
    if(r != DesfireResultOk) return r;

    r = df_create_app(nfc, cfg->app_id, cfg->key_index);
    if(r != DesfireResultOk) return r;

    r = df_select_app(nfc, cfg->app_id);
    if(r != DesfireResultOk) return r;

    r = df_authenticate_aes(nfc, cfg->key_index, cfg->app_key);
    bool used_custom_key = true;
    if(r == DesfireResultAuthFailed) {
        r = df_authenticate_aes(nfc, cfg->key_index, ZERO_KEY);
        used_custom_key = false;
        if(r != DesfireResultOk) {
            FURI_LOG_E(TAG, "Auth failed with both custom and default key");
            return DesfireResultAuthFailed;
        }
    } else if(r != DesfireResultOk) {
        return r;
    }

    r = df_create_file(nfc, cfg->file_id, cfg->key_index);
    if(r != DesfireResultOk) return r;

    if(cfg->change_key && !used_custom_key) {
        r = df_change_key_simple(nfc, cfg->key_index, cfg->app_key);
        if(r != DesfireResultOk) return r;
    }

    // Write CID using encrypted write (re-auths to get clean IV state)
    r = desfire_akuvox_write_cid(nfc, cfg, cid);
    return r;
}

DesfireResult desfire_akuvox_read_data(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint8_t* data_out,
    size_t len) {
    DesfireResult r;

    r = df_select_app(nfc, cfg->app_id);
    if(r != DesfireResultOk) return r;

    r = df_authenticate_aes(nfc, cfg->key_index, cfg->app_key);
    if(r == DesfireResultAuthFailed) {
        r = df_authenticate_aes(nfc, cfg->key_index, ZERO_KEY);
        if(r != DesfireResultOk) return DesfireResultAuthFailed;
    } else if(r != DesfireResultOk) {
        return r;
    }

    // Log session key for debugging
    {
        char hex[48];
        int p = 0;
        for(int i = 0; i < 16; i++)
            p += snprintf(hex + p, sizeof(hex) - p, "%02X ", session.key[i]);
        FURI_LOG_I(TAG, "SesKey: %s", hex);
    }

    // Known Akuvox file settings: comm=0x03 (encrypted), size=8
    // Skip GetFileSettings to avoid CMAC tracking complexity for that command.
    // ReadData is now the FIRST command after auth (IV starts at 0).
    uint32_t file_size = 8;

    // --- ReadData ---
    uint8_t cmd[8];
    cmd[0] = DF_CMD_READ_DATA;
    cmd[1] = cfg->file_id;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00;
    cmd[5] = (uint8_t)(file_size & 0xFF);
    cmd[6] = 0x00;
    cmd[7] = 0x00;

    // CMAC over command updates IV (card does the same before encrypting response)
    df_cmac(cmd, sizeof(cmd));

    // Log IV that will be used for decryption
    {
        char hex[48];
        int p = 0;
        for(int i = 0; i < 16; i++)
            p += snprintf(hex + p, sizeof(hex) - p, "%02X ", session.iv[i]);
        FURI_LOG_I(TAG, "DecIV: %s", hex);
    }

    uint8_t raw[64];
    size_t raw_len = sizeof(raw);
    uint8_t st;
    if(!df_native_cmd(nfc, cmd, sizeof(cmd), raw, &raw_len, &st))
        return DesfireResultTransport;

    if(st != DF_STATUS_OK) {
        FURI_LOG_E(TAG, "ReadData error: %02X", st);
        return DesfireResultVerifyFailed;
    }

    // Encrypted response: decrypt full 16-byte-aligned block(s)
    // Plaintext layout: [data (file_size)] [CRC32 (4)] [padding to 16B]
    if(session.active && raw_len >= 16) {
        uint8_t plain[64];
        size_t dec_len = raw_len & ~(size_t)15;
        if(dec_len == 0) {
            FURI_LOG_E(TAG, "Encrypted response too short: %zu", raw_len);
            return DesfireResultVerifyFailed;
        }
        df_decrypt_session(raw, plain, dec_len);

        char hex[128];
        int p = 0;
        for(size_t i = 0; i < (dec_len > 16 ? 16 : dec_len); i++)
            p += snprintf(hex + p, sizeof(hex) - p, "%02X ", plain[i]);
        FURI_LOG_I(TAG, "Decrypted(%zu): %s", dec_len, hex);

        size_t copy_len = (len < file_size) ? len : file_size;
        memcpy(data_out, plain, copy_len);
    } else {
        // Fallback plain
        size_t copy_len = (len < raw_len) ? len : raw_len;
        memcpy(data_out, raw, copy_len);
    }

    return DesfireResultOk;
}

DesfireResult desfire_akuvox_read_cid(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t* cid_out) {
    uint8_t data[4];
    DesfireResult r = desfire_akuvox_read_data(nfc, cfg, data, 4);
    if(r != DesfireResultOk) return r;

    *cid_out = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    return DesfireResultOk;
}

const char* desfire_result_name(DesfireResult r) {
    switch(r) {
    case DesfireResultOk: return "OK";
    case DesfireResultTransport: return "Transport error";
    case DesfireResultSelectAppFailed: return "Select app failed";
    case DesfireResultCreateAppFailed: return "Create app failed";
    case DesfireResultCreateFileFailed: return "Create file failed";
    case DesfireResultAuthFailed: return "Auth failed";
    case DesfireResultChangeKeyFailed: return "Change key failed";
    case DesfireResultWriteFailed: return "Write failed";
    case DesfireResultVerifyFailed: return "Verify failed";
    case DesfireResultAlreadyPersonalised: return "Already personalised";
    default: return "Unknown";
    }
}
