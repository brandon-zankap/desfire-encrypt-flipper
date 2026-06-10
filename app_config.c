#include "app_config.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CONFIG_DIR  EXT_PATH("apps_data/desfire_seq_writer")
#define CONFIG_PATH EXT_PATH("apps_data/desfire_seq_writer/config.txt")

static bool hex_to_bytes(const char* hex, uint8_t* out, size_t out_len) {
    size_t n = strlen(hex);
    if(n != out_len * 2) return false;
    for(size_t i = 0; i < out_len; i++) {
        unsigned int v;
        if(sscanf(hex + i * 2, "%2x", &v) != 1) return false;
        out[i] = (uint8_t)v;
    }
    return true;
}

void desfire_seq_config_defaults(DesfireSeqConfig* cfg) {
    memset(cfg, 0, sizeof(DesfireSeqConfig));
    cfg->app_id    = 0x000045;
    cfg->file_id   = 0x00;
    cfg->key_index = 0x00;
    memset(cfg->app_key, 0x69, DESFIRE_KEY_LEN);
    cfg->start_cid   = 1;
    cfg->end_cid     = 1000;
    cfg->change_key  = true;
    cfg->decimal_cid = false;
}

bool desfire_seq_config_save(const DesfireSeqConfig* cfg) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, CONFIG_DIR);
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        char key_hex[33];
        for(size_t i = 0; i < DESFIRE_KEY_LEN; i++)
            snprintf(key_hex + i * 2, 3, "%02X", cfg->app_key[i]);
        char buf[256];
        int len = snprintf(
            buf, sizeof(buf),
            "app_id=%06lX\nfile_id=%u\nkey_index=%u\napp_key=%s\n"
            "start_cid=%lu\nend_cid=%lu\nchange_key=%s\ndecimal_cid=%s\n",
            cfg->app_id,
            cfg->file_id,
            cfg->key_index,
            key_hex,
            cfg->start_cid,
            cfg->end_cid,
            cfg->change_key ? "true" : "false",
            cfg->decimal_cid ? "true" : "false");
        if(len > 0) storage_file_write(file, buf, (size_t)len);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool desfire_seq_config_load(DesfireSeqConfig* cfg) {
    desfire_seq_config_defaults(cfg);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    char buf[512] = {0};
    uint16_t len = storage_file_read(file, buf, sizeof(buf) - 1);
    buf[len] = 0;
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    char* p = buf;
    while(*p) {
        // Find end of line
        char* eol = p;
        while(*eol && *eol != '\n' && *eol != '\r') eol++;
        char saved = *eol;
        *eol = '\0';

        char* eq = strchr(p, '=');
        if(eq) {
            *eq = '\0';
            const char* k = p;
            const char* v = eq + 1;
            if(strcmp(k, "app_id") == 0)
                cfg->app_id = strtoul(v, NULL, 16) & 0xFFFFFF;
            else if(strcmp(k, "file_id") == 0)
                cfg->file_id = (uint8_t)strtoul(v, NULL, 10) & 0x1F;
            else if(strcmp(k, "key_index") == 0)
                cfg->key_index = (uint8_t)strtoul(v, NULL, 10);
            else if(strcmp(k, "app_key") == 0)
                hex_to_bytes(v, cfg->app_key, DESFIRE_KEY_LEN);
            else if(strcmp(k, "start_cid") == 0)
                cfg->start_cid = strtoul(v, NULL, 10);
            else if(strcmp(k, "end_cid") == 0)
                cfg->end_cid = strtoul(v, NULL, 10);
            else if(strcmp(k, "change_key") == 0)
                cfg->change_key = strcmp(v, "false") != 0;
            else if(strcmp(k, "decimal_cid") == 0)
                cfg->decimal_cid = strcmp(v, "false") != 0;
        }

        if(!saved) break;
        p = eol + 1;
        while(*p == '\n' || *p == '\r') p++;
    }
    if(cfg->key_index > 13) cfg->key_index = 0;
    return true;
}
