#include <stdio.h>
#include <string.h>

#include "include/textui.h"
#include "include/textui_input.h"
#include "include/textui_dialog.h"

#include "include/vn_app.h"
#include "include/vn_config.h"
#include "include/vn_config_ui.h"
#include "include/vn_protocol_test.h"
#include "include/vn_serial.h"
#include "include/vn_ui.h"

#define VN_SERIAL_BACKEND_ENABLED 0

static int vn_app_port_one_selected(const VnConfig *config)
{
    int i;

    if (config == 0)
        return 0;
    for (i = 0; config->ports[i] != '\0'; i++)
    {
        if (config->ports[i] == '1' &&
            (i == 0 || config->ports[i - 1] == ',') &&
            (config->ports[i + 1] == '\0' ||
             config->ports[i + 1] == ','))
            return 1;
    }
    return 0;
}

static int vn_app_open_serial(const VnConfig *config)
{
#if VN_SERIAL_BACKEND_ENABLED
    vn_serial_close();
    if (!vn_app_port_one_selected(config))
        return 0;
    return vn_serial_open(1, config->baud);
#else
    (void)config;
    return 0;
#endif
}

static void vn_app_show_serial_result(const VnConfig *config)
{
    static char message[96];

    if (!vn_app_port_one_selected(config))
    {
        textui_message_dialog("SERIAL DISABLED",
                              "Select port 1 to enable serial.");
        return;
    }

#if !VN_SERIAL_BACKEND_ENABLED
    textui_message_dialog("SERIAL DISABLED",
                          "Firmware backend temporarily disabled.");
    return;
#endif

    if (vn_serial_status() == VN_SERIAL_STATUS_OPEN)
    {
        sprintf(message, "Printer port open.\n%ld baud, 8N1.",
                config->baud);
        textui_message_dialog("SERIAL OPEN", message);
    }
    else
    {
        sprintf(message, "Printer port failed: %s.",
                vn_serial_status_text());
        textui_message_dialog("SERIAL ERROR", message);
    }
}

static void vn_app_run_protocol_test(void)
{
    static VnProtocolTestResult result;
    static char message[96];

    vn_protocol_test_run(&result);
    if (!vn_serial_ring_self_test())
    {
        textui_message_dialog("SERIAL TEST",
                              "Ring-buffer self-test failed.");
        return;
    }
    if (result.failed == 0)
    {
        sprintf(message, "PASSED %d OF %d TESTS.\nPROTOCOL OK.",
                result.passed,
                result.total);
        textui_message_dialog("PROTOCOL TEST", message);
    }
    else
    {
        sprintf(message, "FAILED %d OF %d TESTS.\nFIRST: %.40s",
                result.failed,
                result.total,
                result.first_failed);
        textui_message_dialog("PROTOCOL TEST", message);
    }
}

static int vn_app_serial_stats_changed(const VnSerialStats *before,
                                       const VnSerialStats *after,
                                       VnSerialStatus before_status,
                                       VnSerialStatus after_status)
{
    if (before_status != after_status)
        return 1;
    return memcmp(before, after, sizeof(*before)) != 0;
}

static void vn_app_run_serial_diagnostics(const VnConfig *config,
                                          int serial_configured)
{
    TextUiKeyEvent event;
    static VnSerialStats displayed_stats;
    VnSerialStatus displayed_status;
    unsigned char value;

    displayed_stats = *vn_serial_stats();
    displayed_status = vn_serial_status();
    vn_ui_draw_serial_diagnostics(config->baud, serial_configured,
                                  VN_SERIAL_BACKEND_ENABLED);

    while (1)
    {
        vn_serial_poll();
        while (vn_serial_read_byte(&value) > 0)
            ;

        if (vn_app_serial_stats_changed(&displayed_stats,
                                        vn_serial_stats(),
                                        displayed_status,
                                        vn_serial_status()))
        {
            displayed_stats = *vn_serial_stats();
            displayed_status = vn_serial_status();
            vn_ui_draw_serial_diagnostics(config->baud,
                                          serial_configured,
                                          VN_SERIAL_BACKEND_ENABLED);
        }

        if (textui_poll_key_event(&event) &&
            (event.key == TEXTUI_KEY_ESCAPE ||
             event.ch == 'Q' || event.ch == 'q'))
            break;
    }
}

int vn_app_run(void)
{
    static VnConfig config;
    static VnConfigStatus status;
    TextUiKeyEvent event;
    VnSerialStatus displayed_serial_status;
    int displayed_serial_error;
    int serial_configured;
    unsigned char value;

    textui_init();
    vn_config_load(VN_CONFIG_DEFAULT_FILE, &config, &status);
    serial_configured = vn_app_port_one_selected(&config);
    vn_ui_draw_shell(&config, &status, serial_configured,
                     VN_SERIAL_BACKEND_ENABLED);
    vn_config_ui_show_startup_status(&status);
    vn_app_open_serial(&config);
    displayed_serial_status = vn_serial_status();
    displayed_serial_error = vn_serial_stats()->last_error;
    vn_ui_draw_shell(&config, &status, serial_configured,
                     VN_SERIAL_BACKEND_ENABLED);

    while (1)
    {
        vn_serial_poll();
        while (vn_serial_read_byte(&value) > 0)
            ;

        if (displayed_serial_status != vn_serial_status() ||
            displayed_serial_error != vn_serial_stats()->last_error)
        {
            displayed_serial_status = vn_serial_status();
            displayed_serial_error = vn_serial_stats()->last_error;
            vn_ui_draw_shell(&config, &status, serial_configured,
                             VN_SERIAL_BACKEND_ENABLED);
        }

        if (!textui_poll_key_event(&event))
            continue;
        if (event.key == TEXTUI_KEY_ESCAPE ||
            event.ch == 'Q' ||
            event.ch == 'q')
            break;

        if (event.ch == 'C' || event.ch == 'c')
        {
            if (vn_config_ui_run_wizard(&config, &status))
            {
                serial_configured = vn_app_port_one_selected(&config);
                vn_app_open_serial(&config);
                vn_app_show_serial_result(&config);
                displayed_serial_status = vn_serial_status();
                displayed_serial_error = vn_serial_stats()->last_error;
            }
            vn_ui_draw_shell(&config, &status, serial_configured,
                             VN_SERIAL_BACKEND_ENABLED);
        }
        else if (event.ch == 'T' || event.ch == 't')
        {
            vn_app_run_protocol_test();
            vn_ui_draw_shell(&config, &status, serial_configured,
                             VN_SERIAL_BACKEND_ENABLED);
        }
        else if (event.ch == 'D' || event.ch == 'd')
        {
            vn_app_run_serial_diagnostics(&config, serial_configured);
            displayed_serial_status = vn_serial_status();
            displayed_serial_error = vn_serial_stats()->last_error;
            vn_ui_draw_shell(&config, &status, serial_configured,
                             VN_SERIAL_BACKEND_ENABLED);
        }
    }

    vn_serial_close();
    textui_restore();
    return 0;
}
