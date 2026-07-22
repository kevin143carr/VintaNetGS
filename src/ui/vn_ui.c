#include <stdio.h>

#include "include/vn_version.h"
#include "include/textui.h"
#include "include/vn_config.h"

static void vn_ui_draw_label_status(int row, const char *label, const char *status)
{
    textui_write_field(3, row, 14, label, TEXTUI_NORMAL);
    textui_write_field(18, row, 19, status, TEXTUI_NORMAL);
}

static void vn_ui_write_number_status(int row, const char *label, long value)
{
    char text[20];

    sprintf(text, "%ld", value);
    vn_ui_draw_label_status(row, label, text);
}

static void vn_ui_write_setup_value(int row, const char *label, const char *value)
{
    if (value == 0 || value[0] == '\0')
        vn_ui_draw_label_status(row, label, "SETUP NEEDED");
    else
        vn_ui_draw_label_status(row, label, value);
}

void vn_ui_draw_shell(const VnConfig *config, const VnConfigStatus *status)
{
    char detail[34];

    textui_clear();

    textui_write_field(0, 0, 40, VN_APP_NAME, TEXTUI_INVERSE);
    textui_draw_box(0, 1, 40, 22);
    textui_write_field(0, 23, 40, "C: CONFIG  T: TEST  ESC/Q: EXIT", TEXTUI_INVERSE);

    textui_write_field(3, 3, 34, VN_APP_NAME, TEXTUI_NORMAL);
    textui_write_field(3, 4, 34, "APPLE IIGS SERIAL NETWORK", TEXTUI_NORMAL);
    textui_write_field(3, 6, 34, "DEVELOPMENT SHELL", TEXTUI_INVERSE);

    vn_ui_draw_label_status(9, "UI", "READY");
    vn_ui_draw_label_status(10, "CONFIG", vn_config_result_text(status->result));
    vn_ui_write_setup_value(11, "MACHINE", config->machine);
    vn_ui_draw_label_status(12, "ROLE", config->role);
    vn_ui_write_setup_value(13, "PORTS", config->ports);
    vn_ui_write_number_status(14, "BAUD", config->baud);
    vn_ui_write_number_status(15, "CAPABILITIES", (long)config->capability_count);

    if (status->result == VN_CONFIG_INVALID_VALUE ||
        status->result == VN_CONFIG_INVALID_FORMAT ||
        status->result == VN_CONFIG_READ_ERROR ||
        status->result == VN_CONFIG_OPEN_ERROR ||
        status->result == VN_CONFIG_WRITE_ERROR)
    {
        sprintf(detail, "LINE %d %.20s", status->line, status->key);
        textui_write_field(3, 17, 34, detail, TEXTUI_INVERSE);
        textui_write_field(3, 18, 34, status->message, TEXTUI_NORMAL);
    }
    else if (vn_config_needs_setup(config))
    {
        textui_write_field(3, 17, 34, "SETUP STAGE REQUIRED", TEXTUI_INVERSE);
        textui_write_field(3, 18, 34, "PRESS C TO CONFIGURE", TEXTUI_NORMAL);
    }

    vn_ui_draw_label_status(19, "SERIAL", "NOT IMPLEMENTED");
    vn_ui_draw_label_status(20, "COMMUNICATIONS", "NOT IMPLEMENTED");

    textui_write_field(3, 21, 34, "VERSION " VN_VERSION_TEXT, TEXTUI_NORMAL);

    textui_present();
}
