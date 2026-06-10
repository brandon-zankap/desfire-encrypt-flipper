#include "desfire_seq_writer.h"
#include "scenes.h"

// ── Scene handler table ───────────────────────────────────

void (*const desfire_seq_scene_on_enter_handlers[])(void*) = {
    scene_main_on_enter,
    scene_config_on_enter,
    scene_running_on_enter,
    scene_result_on_enter,
    scene_read_test_on_enter,
    scene_write_one_on_enter,
    scene_write_one_run_on_enter,
};

bool (*const desfire_seq_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    scene_main_on_event,
    scene_config_on_event,
    scene_running_on_event,
    scene_result_on_event,
    scene_read_test_on_event,
    scene_write_one_on_event,
    scene_write_one_run_on_event,
};

void (*const desfire_seq_scene_on_exit_handlers[])(void*) = {
    scene_main_on_exit,
    scene_config_on_exit,
    scene_running_on_exit,
    scene_result_on_exit,
    scene_read_test_on_exit,
    scene_write_one_on_exit,
    scene_write_one_run_on_exit,
};

const SceneManagerHandlers desfire_seq_scene_handlers = {
    .on_enter_handlers = desfire_seq_scene_on_enter_handlers,
    .on_event_handlers = desfire_seq_scene_on_event_handlers,
    .on_exit_handlers  = desfire_seq_scene_on_exit_handlers,
    .scene_num         = DesfireSeqSceneCount,
};

// ── View dispatcher callbacks ─────────────────────────────

static bool desfire_seq_back_event_callback(void* ctx) {
    DesfireSeqApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool desfire_seq_custom_event_callback(void* ctx, uint32_t event) {
    DesfireSeqApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool desfire_seq_view_input_callback(InputEvent* event, void* ctx) {
    DesfireSeqApp* app = ctx;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            return scene_manager_handle_custom_event(app->scene_manager, EventKeyUp);
        } else if(event->key == InputKeyDown) {
            return scene_manager_handle_custom_event(app->scene_manager, EventKeyDown);
        } else if(event->key == InputKeyOk) {
            return scene_manager_handle_custom_event(app->scene_manager, EventKeyOk);
        }
    }
    return false;
}

// ── App alloc/free ────────────────────────────────────────

DesfireSeqApp* desfire_seq_app_alloc(void) {
    DesfireSeqApp* app = malloc(sizeof(DesfireSeqApp));
    memset(app, 0, sizeof(DesfireSeqApp));

    // Load config from SD card, or create defaults
    if(!desfire_seq_config_load(&app->cfg)) {
        desfire_seq_config_defaults(&app->cfg);
        desfire_seq_config_save(&app->cfg);
    }

    // Framework
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, desfire_seq_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, desfire_seq_back_event_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&desfire_seq_scene_handlers, app);

    // Views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewConfig,
        variable_item_list_get_view(app->var_item_list));

    app->widget_running = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewRunning, widget_get_view(app->widget_running));
    view_set_context(widget_get_view(app->widget_running), app);
    view_set_input_callback(
        widget_get_view(app->widget_running), desfire_seq_view_input_callback);

    app->widget_result = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewResult, widget_get_view(app->widget_result));
    view_set_context(widget_get_view(app->widget_result), app);
    view_set_input_callback(
        widget_get_view(app->widget_result), desfire_seq_view_input_callback);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewTextInput, text_input_get_view(app->text_input));

    app->widget_read_test = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewReadTest, widget_get_view(app->widget_read_test));
    view_set_context(widget_get_view(app->widget_read_test), app);
    view_set_input_callback(
        widget_get_view(app->widget_read_test), desfire_seq_view_input_callback);

    app->widget_write_one = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewWriteOne, widget_get_view(app->widget_write_one));
    view_set_context(widget_get_view(app->widget_write_one), app);
    view_set_input_callback(
        widget_get_view(app->widget_write_one), desfire_seq_view_input_callback);

    app->byte_input_hex = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DesfireSeqViewByteInput,
        byte_input_get_view(app->byte_input_hex));

    return app;
}

void desfire_seq_app_free(DesfireSeqApp* app) {
    furi_assert(app);

    scene_manager_free(app->scene_manager);

    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewRunning);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewReadTest);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewWriteOne);
    view_dispatcher_remove_view(app->view_dispatcher, DesfireSeqViewByteInput);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget_running);
    widget_free(app->widget_result);
    widget_free(app->widget_read_test);
    widget_free(app->widget_write_one);
    text_input_free(app->text_input);
    byte_input_free(app->byte_input_hex);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ── Entry point ───────────────────────────────────────────

int32_t desfire_seq_writer_app(void* p) {
    UNUSED(p);

    DesfireSeqApp* app = desfire_seq_app_alloc();

    scene_manager_next_scene(app->scene_manager, DesfireSeqSceneMain);
    view_dispatcher_run(app->view_dispatcher);

    desfire_seq_app_free(app);
    return 0;
}
