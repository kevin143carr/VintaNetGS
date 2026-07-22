#include <stdio.h>

#include "include/vn_version.h"
#include "include/textui.h"
#include "include/vn_config.h"
#include "include/vn_serial.h"

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

static void vn_ui_format_serial_status(char *text,
                                       const VnConfig *config,
                                       int serial_configured,
                                       int serial_backend_enabled)
{
    if (!serial_configured)
    {
        sprintf(text, "SERIAL: SLOT 1 CONFIG REQUIRED");
        return;
    }

    if (!serial_backend_enabled)
    {
        sprintf(text, "SERIAL: SLOT 1 BACKEND DISABLED");
        return;
    }

    sprintf(text, "SERIAL: SLOT 1 %ld 8N1 %s",
            config->baud, vn_serial_status_text());
}

static void vn_ui_format_history(char *text,
                                 const unsigned char *history,
                                 unsigned int count)
{
    unsigned int i;
    unsigned int position;

    position = 0;
    for (i = 0; i < VN_SERIAL_HISTORY_SIZE; i++)
    {
        if (i < count)
            sprintf(text + position, "%02X", (unsigned int)history[i]);
        else
            sprintf(text + position, "00");
        position += 2;
        if (i + 1U < VN_SERIAL_HISTORY_SIZE)
        {
            text[position] = ' ';
            position++;
        }
    }
    text[position] = '\0';
}

void vn_ui_draw_shell(const VnConfig *config,
                      const VnConfigStatus *status,
                      int serial_configured,
                      int serial_backend_enabled)
{
    char detail[34];
    char serial_text[38];

    textui_clear();

    textui_write_field(0, 0, 40, VN_APP_NAME, TEXTUI_INVERSE);
    textui_draw_box(0, 1, 40, 22);
    textui_write_field(0, 23, 40,
                       "C:CONFIG D:SERIAL T:TEST ESC/Q:EXIT",
                       TEXTUI_INVERSE);

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

    vn_ui_format_serial_status(serial_text, config, serial_configured,
                               serial_backend_enabled);
    textui_write_field(3, 19, 34, serial_text, TEXTUI_NORMAL);
    textui_write_field(3, 20, 34, "VINTANET: TESTED / IDLE", TEXTUI_NORMAL);

    textui_write_field(3, 21, 34, "VERSION " VN_VERSION_TEXT, TEXTUI_NORMAL);

    textui_present();
}

void vn_ui_draw_serial_diagnostics(long baud,
                                   int serial_configured,
                                   int serial_backend_enabled)
{
    const VnSerialStats *stats;
    static unsigned char rx_history[VN_SERIAL_HISTORY_SIZE];
    static unsigned char tx_history[VN_SERIAL_HISTORY_SIZE];
    static char text[38];
    unsigned int rx_count;
    unsigned int tx_count;

    stats = vn_serial_stats();
    rx_count = vn_serial_recent_rx(rx_history, VN_SERIAL_HISTORY_SIZE);
    tx_count = vn_serial_recent_tx(tx_history, VN_SERIAL_HISTORY_SIZE);

    textui_clear();
    textui_write_field(0, 0, 40, "SERIAL DIAGNOSTICS", TEXTUI_INVERSE);
    textui_draw_box(0, 1, 40, 22);
    textui_write_field(0, 23, 40, "ESC: RETURN", TEXTUI_INVERSE);

    textui_write_field(3, 3, 34, "PORT:   PRINTER / SLOT 1", TEXTUI_NORMAL);
    sprintf(text, "FORMAT: %ld 8N1", baud);
    textui_write_field(3, 4, 34, text, TEXTUI_NORMAL);
    if (!serial_configured)
        sprintf(text, "STATE:  CONFIG REQUIRED");
    else if (!serial_backend_enabled)
        sprintf(text, "STATE:  BACKEND DISABLED");
    else
        sprintf(text, "STATE:  %s", vn_serial_status_text());
    textui_write_field(3, 5, 34, text, TEXTUI_NORMAL);

    sprintf(text, "RX BYTES:       %08lu", stats->rx_bytes);
    textui_write_field(3, 7, 34, text, TEXTUI_NORMAL);
    sprintf(text, "TX BYTES:       %08lu", stats->tx_bytes);
    textui_write_field(3, 8, 34, text, TEXTUI_NORMAL);
    sprintf(text, "RX QUEUED:      %04u", stats->rx_queued);
    textui_write_field(3, 9, 34, text, TEXTUI_NORMAL);
    sprintf(text, "TX QUEUED:      %04u", stats->tx_queued);
    textui_write_field(3, 10, 34, text, TEXTUI_NORMAL);
    sprintf(text, "RX OVERFLOW:    %04u", stats->rx_overflows);
    textui_write_field(3, 11, 34, text, TEXTUI_NORMAL);
    sprintf(text, "TX OVERFLOW:    %04u", stats->tx_overflows);
    textui_write_field(3, 12, 34, text, TEXTUI_NORMAL);
    if (stats->last_error == 0)
        sprintf(text, "LAST ERROR:     NONE");
    else
        sprintf(text, "LAST ERROR:     %02X",
                (unsigned int)stats->last_error & 0xFFU);
    textui_write_field(3, 13, 34, text, TEXTUI_NORMAL);

    textui_write_field(3, 15, 34, "RECENT RX:", TEXTUI_NORMAL);
    vn_ui_format_history(text, rx_history, rx_count);
    textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
    textui_write_field(3, 18, 34, "RECENT TX:", TEXTUI_NORMAL);
    vn_ui_format_history(text, tx_history, tx_count);
    textui_write_field(3, 19, 34, text, TEXTUI_NORMAL);

    sprintf(text, "HIGH WATER: RX %04u TX %04u",
            stats->rx_high_water, stats->tx_high_water);
    textui_write_field(3, 21, 34, text, TEXTUI_NORMAL);
    textui_present();
}
