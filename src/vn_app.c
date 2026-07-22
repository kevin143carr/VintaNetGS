#include <stdio.h>

#include "include/textui.h"
#include "include/textui_input.h"
#include "include/textui_dialog.h"

#include "include/vn_app.h"
#include "include/vn_config.h"
#include "include/vn_config_ui.h"
#include "include/vn_protocol_test.h"
#include "include/vn_ui.h"

static void vn_app_run_protocol_test(void)
{
    static VnProtocolTestResult result;
    static char message[96];

    vn_protocol_test_run(&result);
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

int vn_app_run(void)
{
    static VnConfig config;
    static VnConfigStatus status;
    TextUiKeyEvent event;

    textui_init();
    vn_config_load(VN_CONFIG_DEFAULT_FILE, &config, &status);
    vn_ui_draw_shell(&config, &status);
    vn_config_ui_show_startup_status(&status);
    vn_ui_draw_shell(&config, &status);

    while (1)
    {
        textui_read_key_event(&event);
        if (event.key == TEXTUI_KEY_ESCAPE ||
            event.ch == 'Q' ||
            event.ch == 'q')
            break;

        if (event.ch == 'C' || event.ch == 'c')
        {
            vn_config_ui_run_wizard(&config, &status);
            vn_ui_draw_shell(&config, &status);
        }
        else if (event.ch == 'T' || event.ch == 't')
        {
            vn_app_run_protocol_test();
            vn_ui_draw_shell(&config, &status);
        }
    }

    textui_restore();
    return 0;
}
