#include "scenes.h"
#include "desfire_akuvox.h"
#include "nfc_transport.h"
#include "logger.h"
#include <ctype.h>

// ── Helpers ─────────────────────────────────────────────

static bool parse_hex_bytes(const char* hex, uint8_t* out, size_t out_len) {
    if(strlen(hex) != out_len * 2) return false;
    for(size_t i = 0; i < out_len; i++) {
        char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        char* endp;
        long val = strtol(byte_str, &endp, 16);
        if(*endp != '\0') return false;
        out[i] = (uint8_t)val;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  SCENE: MAIN MENU
// ═══════════════════════════════════════════════════════════

typedef enum {
    SubmenuIndexConfig = 0,
    SubmenuIndexStart,
    SubmenuIndexReadTest,
    SubmenuIndexWriteOne,
} SubmenuIndex;

static void submenu_callback(void* ctx, uint32_t index) {
    DesfireSeqApp* app = ctx;
    if(index == SubmenuIndexConfig) {
        scene_manager_next_scene(app->scene_manager, DesfireSeqSceneConfig);
    } else if(index == SubmenuIndexStart) {
        scene_manager_next_scene(app->scene_manager, DesfireSeqSceneRunning);
    } else if(index == SubmenuIndexReadTest) {
        scene_manager_next_scene(app->scene_manager, DesfireSeqSceneReadTest);
    } else if(index == SubmenuIndexWriteOne) {
        scene_manager_next_scene(app->scene_manager, DesfireSeqSceneWriteOne);
    }
}

void scene_main_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "DESFire Seq Writer");
    submenu_add_item(app->submenu, "Configure", SubmenuIndexConfig, submenu_callback, app);
    submenu_add_item(app->submenu, "Start Writing", SubmenuIndexStart, submenu_callback, app);
    submenu_add_item(app->submenu, "Read/Test Tag", SubmenuIndexReadTest, submenu_callback, app);
    submenu_add_item(app->submenu, "Write One CID", SubmenuIndexWriteOne, submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewSubmenu);
}

bool scene_main_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void scene_main_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;
    submenu_reset(app->submenu);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: CONFIG
// ═══════════════════════════════════════════════════════════

static void config_refresh(DesfireSeqApp* app) {
    widget_reset(app->widget_config);
    char line[64];

    widget_add_string_element(
        app->widget_config, 0, 0, AlignLeft, AlignTop, FontPrimary, "[ Config ]");

    // Line 1: AID & File ID
    snprintf(
        line,
        sizeof(line),
        "%sAID:%06lX %sFile:%u",
        app->edit_field == 0 ? ">" : " ",
        app->cfg.app_id,
        app->edit_field == 1 ? ">" : " ",
        app->cfg.file_id);
    widget_add_string_element(app->widget_config, 0, 13, AlignLeft, AlignTop, FontSecondary, line);

    // Line 2: Key Index & Change Key
    snprintf(
        line,
        sizeof(line),
        "%sKeyIdx:%u %sChgKey:%s",
        app->edit_field == 2 ? ">" : " ",
        app->cfg.key_index,
        app->edit_field == 6 ? ">" : " ",
        app->cfg.change_key ? "Y" : "N");
    widget_add_string_element(app->widget_config, 0, 23, AlignLeft, AlignTop, FontSecondary, line);

    // Line 3: Key
    char key_short[12];
    for(int i = 0; i < 4; i++) snprintf(key_short + i * 2, 3, "%02X", app->cfg.app_key[i]);
    snprintf(line, sizeof(line), "%sKey: %s...", app->edit_field == 3 ? ">" : " ", key_short);
    widget_add_string_element(app->widget_config, 0, 33, AlignLeft, AlignTop, FontSecondary, line);

    // Line 4: CID Range
    snprintf(
        line,
        sizeof(line),
        "%sStart:%08lX %sEnd:%08lX",
        app->edit_field == 4 ? ">" : " ",
        app->cfg.start_cid,
        app->edit_field == 5 ? ">" : " ",
        app->cfg.end_cid);
    widget_add_string_element(app->widget_config, 0, 43, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        app->widget_config, 0, 55, AlignLeft, AlignTop, FontSecondary, "Up/Dn=Sel OK=Edit BACK=Ret");
}

// Text input callbacks for each field
static void ti_save_app_id(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 16);
    if(*endp == '\0' && val <= 0xFFFFFF) app->cfg.app_id = val;
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_file_id(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 10);
    if(*endp == '\0' && val <= 16) app->cfg.file_id = (uint8_t)val;
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_key_index(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 10);
    if(*endp == '\0' && val <= 13) app->cfg.key_index = (uint8_t)val;
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_app_key(void* ctx) {
    DesfireSeqApp* app = ctx;
    for(size_t i = 0; i < strlen(app->text_buf); i++)
        app->text_buf[i] = (char)toupper((unsigned char)app->text_buf[i]);
    uint8_t tmp[16];
    if(parse_hex_bytes(app->text_buf, tmp, 16)) memcpy(app->cfg.app_key, tmp, 16);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_start_cid(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 10);
    if(*endp == '\0') {
        app->cfg.start_cid = val;
        if(app->cfg.start_cid > app->cfg.end_cid) app->cfg.end_cid = app->cfg.start_cid;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_end_cid(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 10);
    if(*endp == '\0') app->cfg.end_cid = val;
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void ti_save_change_key(void* ctx) {
    DesfireSeqApp* app = ctx;
    for(size_t i = 0; i < strlen(app->text_buf); i++)
        app->text_buf[i] = (char)tolower((unsigned char)app->text_buf[i]);
    app->cfg.change_key =
        (strcmp(app->text_buf, "y") == 0 || strcmp(app->text_buf, "yes") == 0 ||
         strcmp(app->text_buf, "true") == 0 || strcmp(app->text_buf, "1") == 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
    config_refresh(app);
}

static void config_edit_field(DesfireSeqApp* app, uint8_t field) {
    app->edit_field = field;
    text_input_reset(app->text_input);

    const char* header = "";
    void (*save_cb)(void*) = NULL;
    uint8_t max_len = 12;

    switch(field) {
    case 0:
        header = "App ID (6 hex chars)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%06lX", app->cfg.app_id);
        save_cb = ti_save_app_id;
        max_len = 6;
        break;
    case 1:
        header = "File ID (0-16)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%u", app->cfg.file_id);
        save_cb = ti_save_file_id;
        max_len = 2;
        break;
    case 2:
        header = "Key Index (0-13)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%u", app->cfg.key_index);
        save_cb = ti_save_key_index;
        max_len = 2;
        break;
    case 3:
        header = "App Key (32 hex chars)";
        for(int i = 0; i < 16; i++)
            snprintf(app->text_buf + i * 2, 3, "%02X", app->cfg.app_key[i]);
        save_cb = ti_save_app_key;
        max_len = 32;
        break;
    case 4:
        header = "Start CID (decimal)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%lu", app->cfg.start_cid);
        save_cb = ti_save_start_cid;
        max_len = 10;
        break;
    case 5:
        header = "End CID (decimal)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%lu", app->cfg.end_cid);
        save_cb = ti_save_end_cid;
        max_len = 10;
        break;
    case 6:
        header = "Change Key? (y/n)";
        snprintf(app->text_buf, sizeof(app->text_buf), "%s", app->cfg.change_key ? "y" : "n");
        save_cb = ti_save_change_key;
        max_len = 5;
        break;
    }

    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, save_cb, app, app->text_buf, max_len + 1, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewTextInput);
}

void scene_config_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;
    config_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewConfig);
}

bool scene_config_on_event(void* ctx, SceneManagerEvent event) {
    DesfireSeqApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventFieldEdit || event.event == EventKeyOk) {
            config_edit_field(app, app->edit_field);
            return true;
        } else if(event.event == EventKeyUp) {
            if(app->edit_field > 0)
                app->edit_field--;
            else
                app->edit_field = 6;
            config_refresh(app);
            return true;
        } else if(event.event == EventKeyDown) {
            if(app->edit_field < 6)
                app->edit_field++;
            else
                app->edit_field = 0;
            config_refresh(app);
            return true;
        }
    }
    return false;
}

void scene_config_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;
    // Save config when leaving config screen
    desfire_seq_config_save(&app->cfg);
    widget_reset(app->widget_config);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: RUNNING — NFC worker thread
// ═══════════════════════════════════════════════════════════

static int32_t nfc_write_worker(void* ctx) {
    DesfireSeqApp* app = ctx;

    app->written_count = 0;
    app->failed_count = 0;
    app->current_cid = app->cfg.start_cid;
    app->last_uid_len = 0;
    memset(app->last_uid, 0, sizeof(app->last_uid));

    while(!app->stop_requested && app->current_cid <= app->cfg.end_cid) {
        NfcTransport nfc;
        NfcTagInfo tag;
        memset(&nfc, 0, sizeof(nfc));
        memset(&tag, 0, sizeof(tag));

        // Try to detect a tag (500ms timeout)
        if(!nfc_transport_open(&nfc, &tag, 500)) {
            furi_delay_ms(100);
            continue;
        }

        // Duplicate detection — skip if same UID as last successful write
        if(tag.uid_len > 0 && tag.uid_len == app->last_uid_len &&
           memcmp(tag.uid, app->last_uid, tag.uid_len) == 0) {
            nfc_transport_close(&nfc);
            furi_delay_ms(200);
            continue;
        }

        snprintf(
            app->status_msg, sizeof(app->status_msg), "Programming CID %lu...", app->current_cid);
        view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteSuccess);

        DesfireResult result = desfire_akuvox_personalise(&nfc, &app->cfg, app->current_cid);

        // Log result
        akuvox_log_result(
            &tag, app->current_cid, result == DesfireResultOk, desfire_result_name(result));

        nfc_transport_close(&nfc);

        if(result == DesfireResultOk) {
            // Remember this UID to detect duplicates
            memcpy(app->last_uid, tag.uid, tag.uid_len);
            app->last_uid_len = tag.uid_len;

            app->written_count++;
            snprintf(
                app->status_msg, sizeof(app->status_msg), "OK: CID %lu", app->current_cid);
            app->current_cid++;
            view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteSuccess);
            furi_delay_ms(800);
        } else {
            app->failed_count++;
            snprintf(app->status_msg, sizeof(app->status_msg), "%s", desfire_result_name(result));
            view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteFail);
            furi_delay_ms(1500);
        }
    }

    app->running = false;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventAllDone);
    return 0;
}

static FuriThread* write_thread = NULL;

static void running_refresh(DesfireSeqApp* app) {
    widget_reset(app->widget_running);
    char line[48];

    widget_add_string_element(
        app->widget_running, 0, 0, AlignLeft, AlignTop, FontPrimary, "Writing...");

    snprintf(
        line,
        sizeof(line),
        "AID:%06lX File:%u Key:%u",
        app->cfg.app_id,
        app->cfg.file_id,
        app->cfg.key_index);
    widget_add_string_element(
        app->widget_running, 0, 13, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Next CID: %08lX", app->current_cid);
    widget_add_string_element(
        app->widget_running, 0, 23, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "OK:%lu  Fail:%lu", app->written_count, app->failed_count);
    widget_add_string_element(
        app->widget_running, 0, 33, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        app->widget_running, 0, 43, AlignLeft, AlignTop, FontSecondary, app->status_msg);

    widget_add_string_element(
        app->widget_running, 0, 55, AlignLeft, AlignTop, FontSecondary, "Hold BACK to stop");
}

void scene_running_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;

    app->running = true;
    app->stop_requested = false;
    app->current_cid = app->cfg.start_cid;
    app->written_count = 0;
    app->failed_count = 0;
    strncpy(app->status_msg, "Place card on Flipper", sizeof(app->status_msg));

    running_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewRunning);

    write_thread = furi_thread_alloc_ex("DesfireSeqWorker", 8192, nfc_write_worker, app);
    furi_thread_start(write_thread);
}

bool scene_running_on_event(void* ctx, SceneManagerEvent event) {
    DesfireSeqApp* app = ctx;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventWriteSuccess || event.event == EventWriteFail) {
            running_refresh(app);
            if(event.event == EventWriteSuccess) {
                notification_message(app->notifications, &sequence_success);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            return true;
        }
        if(event.event == EventAllDone) {
            scene_manager_next_scene(app->scene_manager, DesfireSeqSceneResult);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        app->stop_requested = true;
        return true;
    }

    return false;
}

void scene_running_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;

    if(write_thread) {
        app->stop_requested = true;
        furi_thread_join(write_thread);
        furi_thread_free(write_thread);
        write_thread = NULL;
    }

    widget_reset(app->widget_running);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: RESULT
// ═══════════════════════════════════════════════════════════

void scene_result_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;
    widget_reset(app->widget_result);
    char line[48];

    widget_add_string_element(
        app->widget_result, 0, 0, AlignLeft, AlignTop, FontPrimary, "Done!");

    snprintf(line, sizeof(line), "Written: %lu tags", app->written_count);
    widget_add_string_element(
        app->widget_result, 0, 16, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Failed:  %lu", app->failed_count);
    widget_add_string_element(
        app->widget_result, 0, 26, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line,
        sizeof(line),
        "Last CID: %08lX",
        app->current_cid > app->cfg.start_cid ? app->current_cid - 1 : app->cfg.start_cid);
    widget_add_string_element(
        app->widget_result, 0, 36, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        app->widget_result, 0, 52, AlignLeft, AlignTop, FontSecondary, "Press BACK to menu");

    notification_message(app->notifications, &sequence_success);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewResult);
}

bool scene_result_on_event(void* ctx, SceneManagerEvent event) {
    DesfireSeqApp* app = ctx;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, DesfireSeqSceneMain);
        return true;
    }
    return false;
}

void scene_result_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;
    widget_reset(app->widget_result);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: READ/TEST TAG
// ═══════════════════════════════════════════════════════════

static int32_t nfc_read_worker(void* ctx) {
    DesfireSeqApp* app = ctx;

    strncpy(app->read_status, "Place card on Flipper", sizeof(app->read_status));
    view_dispatcher_send_custom_event(app->view_dispatcher, EventReadDone);

    while(!app->stop_requested) {
        NfcTransport nfc;
        NfcTagInfo tag;
        memset(&nfc, 0, sizeof(nfc));
        memset(&tag, 0, sizeof(tag));

        if(!nfc_transport_open(&nfc, &tag, 500)) {
            furi_delay_ms(100);
            continue;
        }

        strncpy(app->read_status, "Reading...", sizeof(app->read_status));
        view_dispatcher_send_custom_event(app->view_dispatcher, EventReadDone);

        app->read_raw_len = 0;
        memset(app->read_raw, 0, sizeof(app->read_raw));
        DesfireResult result = desfire_akuvox_read_data(&nfc, &app->cfg, app->read_raw, 16);
        if(result != DesfireResultOk) {
            // Fallback to 4 bytes if 16 bytes fails (common for 32-bit CID files)
            result = desfire_akuvox_read_data(&nfc, &app->cfg, app->read_raw, 4);
            if(result == DesfireResultOk) app->read_raw_len = 4;
        } else {
            app->read_raw_len = 16;
        }
        nfc_transport_close(&nfc);

        if(result == DesfireResultOk) {
            FURI_LOG_I("ReadWorker", "Raw data: %02X %02X %02X %02X", 
                       app->read_raw[0], app->read_raw[1], app->read_raw[2], app->read_raw[3]);
            app->read_cid = ((uint32_t)app->read_raw[0] << 24) |
                            ((uint32_t)app->read_raw[1] << 16) |
                            ((uint32_t)app->read_raw[2] << 8) | (uint32_t)app->read_raw[3];
            snprintf(app->read_status, sizeof(app->read_status), "CID: %08lX", app->read_cid);
            notification_message(app->notifications, &sequence_success);
        } else {
            snprintf(app->read_status, sizeof(app->read_status), "%s", desfire_result_name(result));
            notification_message(app->notifications, &sequence_error);
        }

        app->running = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventReadDone);
        return 0;
    }

    app->running = false;
    return 0;
}

static FuriThread* read_thread = NULL;

static void read_test_refresh(DesfireSeqApp* app) {
    widget_reset(app->widget_read_test);
    char line[48];

    widget_add_string_element(
        app->widget_read_test, 0, 0, AlignLeft, AlignTop, FontPrimary, "[ Read/Test Tag ]");

    snprintf(
        line, sizeof(line), "AID:%06lX File:%u Key#:%u",
        app->cfg.app_id, app->cfg.file_id, app->cfg.key_index);
    widget_add_string_element(
        app->widget_read_test, 0, 14, AlignLeft, AlignTop, FontSecondary, line);

    char key_short[12];
    for(int i = 0; i < 4; i++) snprintf(key_short + i * 2, 3, "%02X", app->cfg.app_key[i]);
    snprintf(line, sizeof(line), "Key: %s...", key_short);
    widget_add_string_element(
        app->widget_read_test, 0, 24, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        app->widget_read_test, 0, 32, AlignLeft, AlignTop, FontPrimary, app->read_status);

    if(app->read_raw_len > 0) {
        char hex[64];
        int pos = 0;
        size_t row1 = app->read_raw_len > 8 ? 8 : app->read_raw_len;
        for(size_t i = 0; i < row1; i++) pos += snprintf(hex + pos, 4, "%02X ", app->read_raw[i]);
        widget_add_string_element(app->widget_read_test, 0, 42, AlignLeft, AlignTop, FontSecondary, hex);

        if(app->read_raw_len > 8) {
            pos = 0;
            size_t row2 = app->read_raw_len;
            for(size_t i = 8; i < row2; i++) pos += snprintf(hex + pos, 4, "%02X ", app->read_raw[i]);
            widget_add_string_element(app->widget_read_test, 0, 50, AlignLeft, AlignTop, FontSecondary, hex);
        }
    }

    widget_add_string_element(
        app->widget_read_test, 0, 58, AlignLeft, AlignTop, FontSecondary, "BACK to return");
}

void scene_read_test_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;

    app->running = true;
    app->stop_requested = false;
    app->read_cid = 0;
    strncpy(app->read_status, "Place card on Flipper", sizeof(app->read_status));

    read_test_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewReadTest);

    read_thread = furi_thread_alloc_ex("DesfireReadWorker", 8192, nfc_read_worker, app);
    furi_thread_start(read_thread);
}

bool scene_read_test_on_event(void* ctx, SceneManagerEvent event) {
    DesfireSeqApp* app = ctx;

    if(event.type == SceneManagerEventTypeCustom && event.event == EventReadDone) {
        read_test_refresh(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        app->stop_requested = true;
        return false; // Let scene manager handle the transition
    }

    return false;
}

void scene_read_test_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;

    if(read_thread) {
        app->stop_requested = true;
        furi_thread_join(read_thread);
        furi_thread_free(read_thread);
        read_thread = NULL;
    }

    widget_reset(app->widget_read_test);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: WRITE ONE — CID text input
// ═══════════════════════════════════════════════════════════

static void ti_save_write_one_cid(void* ctx) {
    DesfireSeqApp* app = ctx;
    char* endp;
    uint32_t val = strtoul(app->text_buf, &endp, 16);
    if(*endp == '\0') app->write_one_cid = val;
    scene_manager_next_scene(app->scene_manager, DesfireSeqSceneWriteOneRun);
}

void scene_write_one_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "CID to write (hex)");
    snprintf(app->text_buf, sizeof(app->text_buf), "%lX", app->write_one_cid);
    text_input_set_result_callback(
        app->text_input, ti_save_write_one_cid, app, app->text_buf, 9, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewTextInput);
}

bool scene_write_one_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void scene_write_one_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;
    text_input_reset(app->text_input);
}

// ═══════════════════════════════════════════════════════════
//  SCENE: WRITE ONE RUN — NFC write worker
// ═══════════════════════════════════════════════════════════

static int32_t nfc_write_one_worker(void* ctx) {
    DesfireSeqApp* app = ctx;

    strncpy(app->write_one_status, "Place card on Flipper", sizeof(app->write_one_status));
    view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteOneDone);

    while(!app->stop_requested) {
        NfcTransport nfc;
        NfcTagInfo tag;
        memset(&nfc, 0, sizeof(nfc));
        memset(&tag, 0, sizeof(tag));

        if(!nfc_transport_open(&nfc, &tag, 500)) {
            furi_delay_ms(100);
            continue;
        }

        strncpy(app->write_one_status, "Writing...", sizeof(app->write_one_status));
        view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteOneDone);

        DesfireResult result = desfire_akuvox_write_cid(&nfc, &app->cfg, app->write_one_cid);
        nfc_transport_close(&nfc);

        if(result == DesfireResultOk) {
            snprintf(
                app->write_one_status,
                sizeof(app->write_one_status),
                "OK! CID %08lX verified",
                app->write_one_cid);
            notification_message(app->notifications, &sequence_success);
        } else {
            snprintf(
                app->write_one_status,
                sizeof(app->write_one_status),
                "FAIL: %s",
                desfire_result_name(result));
            notification_message(app->notifications, &sequence_error);
        }

        app->running = false;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventWriteOneDone);
        return 0;
    }

    app->running = false;
    return 0;
}

static FuriThread* write_one_thread = NULL;

static void write_one_refresh(DesfireSeqApp* app) {
    widget_reset(app->widget_write_one);
    char line[48];

    widget_add_string_element(
        app->widget_write_one, 0, 0, AlignLeft, AlignTop, FontPrimary, "[ Write One CID ]");

    snprintf(line, sizeof(line), "CID: 0x%08lX", app->write_one_cid);
    widget_add_string_element(
        app->widget_write_one, 0, 14, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line, sizeof(line), "AID:%06lX File:%u Key#:%u",
        app->cfg.app_id, app->cfg.file_id, app->cfg.key_index);
    widget_add_string_element(
        app->widget_write_one, 0, 24, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_string_element(
        app->widget_write_one, 0, 38, AlignLeft, AlignTop, FontPrimary, app->write_one_status);

    widget_add_string_element(
        app->widget_write_one, 0, 55, AlignLeft, AlignTop, FontSecondary, "BACK to return");
}

void scene_write_one_run_on_enter(void* ctx) {
    DesfireSeqApp* app = ctx;

    app->running = true;
    app->stop_requested = false;
    strncpy(app->write_one_status, "Place card on Flipper", sizeof(app->write_one_status));

    write_one_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesfireSeqViewWriteOne);

    write_one_thread =
        furi_thread_alloc_ex("DesfireWriteOneWorker", 8192, nfc_write_one_worker, app);
    furi_thread_start(write_one_thread);
}

bool scene_write_one_run_on_event(void* ctx, SceneManagerEvent event) {
    DesfireSeqApp* app = ctx;

    if(event.type == SceneManagerEventTypeCustom && event.event == EventWriteOneDone) {
        write_one_refresh(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        app->stop_requested = true;
        return false;
    }

    return false;
}

void scene_write_one_run_on_exit(void* ctx) {
    DesfireSeqApp* app = ctx;

    if(write_one_thread) {
        app->stop_requested = true;
        furi_thread_join(write_one_thread);
        furi_thread_free(write_one_thread);
        write_one_thread = NULL;
    }

    widget_reset(app->widget_write_one);
}
