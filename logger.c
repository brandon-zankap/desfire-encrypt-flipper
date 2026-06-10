#include "logger.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>

#define LOG_DIR EXT_PATH("apps_data/desfire_seq_writer")
#define LOG_PATH EXT_PATH("apps_data/desfire_seq_writer/log.csv")

void akuvox_log_result(const NfcTagInfo* tag, uint32_t cid, bool ok, const char* message) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, LOG_DIR);
    File* file = storage_file_alloc(storage);
    bool new_file = !storage_file_exists(storage, LOG_PATH);
    if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char buf[256];
        int pos = 0;
        if(new_file) {
            const char* hdr = "uid,cid,result,message\n";
            storage_file_write(file, hdr, strlen(hdr));
        }
        for(uint8_t i = 0; i < tag->uid_len; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X", tag->uid[i]);
        pos += snprintf(
            buf + pos, sizeof(buf) - pos, ",%lu,%s,%s\n",
            cid, ok ? "OK" : "FAIL", message ? message : "");
        if(pos > 0) storage_file_write(file, buf, (size_t)pos);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
