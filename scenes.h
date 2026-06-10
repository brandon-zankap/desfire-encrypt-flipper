#pragma once
#include "desfire_seq_writer.h"

// Custom events
typedef enum {
    EventWriteSuccess = 100,
    EventWriteFail,
    EventAllDone,
    EventFieldEdit,
    EventReadDone,
    EventWriteOneDone,
    EventKeyUp,
    EventKeyDown,
    EventKeyOk,
} DesfireSeqCustomEvent;

// Scene on_enter handlers
void scene_main_on_enter(void* ctx);
void scene_config_on_enter(void* ctx);
void scene_running_on_enter(void* ctx);
void scene_result_on_enter(void* ctx);

// Scene on_event handlers
bool scene_main_on_event(void* ctx, SceneManagerEvent event);
bool scene_config_on_event(void* ctx, SceneManagerEvent event);
bool scene_running_on_event(void* ctx, SceneManagerEvent event);
bool scene_result_on_event(void* ctx, SceneManagerEvent event);

// Scene on_exit handlers
void scene_main_on_exit(void* ctx);
void scene_config_on_exit(void* ctx);
void scene_running_on_exit(void* ctx);
void scene_result_on_exit(void* ctx);
void scene_read_test_on_exit(void* ctx);

// Read test scene handlers
void scene_read_test_on_enter(void* ctx);
bool scene_read_test_on_event(void* ctx, SceneManagerEvent event);

// Write one scene handlers
void scene_write_one_on_enter(void* ctx);
bool scene_write_one_on_event(void* ctx, SceneManagerEvent event);
void scene_write_one_on_exit(void* ctx);
void scene_write_one_run_on_enter(void* ctx);
bool scene_write_one_run_on_event(void* ctx, SceneManagerEvent event);
void scene_write_one_run_on_exit(void* ctx);

