#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "app_config.h"

// ── View IDs ──────────────────────────────────────────────
typedef enum {
    DesfireSeqViewSubmenu,
    DesfireSeqViewConfig,
    DesfireSeqViewRunning,
    DesfireSeqViewResult,
    DesfireSeqViewTextInput,
    DesfireSeqViewReadTest,
    DesfireSeqViewWriteOne,
} DesfireSeqView;

// ── Scene IDs ─────────────────────────────────────────────
typedef enum {
    DesfireSeqSceneMain,
    DesfireSeqSceneConfig,
    DesfireSeqSceneRunning,
    DesfireSeqSceneResult,
    DesfireSeqSceneReadTest,
    DesfireSeqSceneWriteOne,
    DesfireSeqSceneWriteOneRun,
    DesfireSeqSceneCount,
} DesfireSeqScene;

// ── App state ─────────────────────────────────────────────
typedef struct {
    // Framework
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    NotificationApp* notifications;

    // Views
    Submenu*    submenu;
    Widget*     widget_config;
    Widget*     widget_running;
    Widget*     widget_result;
    Widget*     widget_read_test;
    Widget*     widget_write_one;
    TextInput*  text_input;

    // Config
    DesfireSeqConfig cfg;

    // Editing state
    uint8_t edit_field; // 0=app_id 1=file_id 2=key_index 3=app_key 4=start 5=end 6=change_key

    // Read test state
    uint32_t read_cid;
    uint8_t  read_raw[16];
    size_t   read_raw_len;
    char     read_status[64];

    // Write one state
    uint32_t write_one_cid;
    char     write_one_status[64];

    // Runtime state
    uint32_t current_cid;
    uint32_t written_count;
    uint32_t failed_count;
    bool     running;
    bool     stop_requested;
    uint8_t  last_uid[10];
    uint8_t  last_uid_len;
    char     status_msg[64];

    // Text input buffer
    char text_buf[40];
} DesfireSeqApp;

// ── Prototypes ────────────────────────────────────────────
DesfireSeqApp* desfire_seq_app_alloc(void);
void           desfire_seq_app_free(DesfireSeqApp* app);
int32_t        desfire_seq_writer_app(void* p);
