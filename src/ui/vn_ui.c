#include <stdio.h>

#include "include/vn_version.h"
#include "include/textui.h"
#include "include/vn_config.h"
#include "include/vn_serial.h"
#include "include/vn_ui.h"

static void vn_ui_format_build_time(char *text)
{
    text[0] = __TIME__[0];
    text[1] = __TIME__[1];
    text[2] = __TIME__[2];
    text[3] = __TIME__[3];
    text[4] = __TIME__[4];
    text[5] = '\0';
}

static void vn_ui_format_title(char *text,
                               const char *title,
                               int include_version)
{
    char time_text[6];

    vn_ui_format_build_time(time_text);
    if (include_version)
        sprintf(text, "%s %s %s %s",
                title, VN_VERSION_TEXT, __DATE__, time_text);
    else
        sprintf(text, "%s %s %s", title, __DATE__, time_text);
}

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

static int vn_ui_probe_has_value(VnSerialProbeOperation operation)
{
    return operation == VN_SERIAL_PROBE_WRITE ||
           operation == VN_SERIAL_PROBE_STATUS_RX ||
           operation == VN_SERIAL_PROBE_STATUS_TX;
}

static const char *vn_ui_probe_outcome_text(VnSerialProbeOutcome outcome)
{
    if (outcome == VN_SERIAL_PROBE_PASSED)
        return "PASS";
    if (outcome == VN_SERIAL_PROBE_INFO)
        return "INFO";
    if (outcome == VN_SERIAL_PROBE_FAILED)
        return "FAIL";
    return "----";
}

static const char *vn_ui_mode_outcome_text(VnSerialModeOutcome outcome)
{
    if (outcome == VN_SERIAL_MODE_IN_FLIGHT)
        return "IN-FLIGHT";
    if (outcome == VN_SERIAL_MODE_PASSED)
        return "PASS";
    if (outcome == VN_SERIAL_MODE_WAIT)
        return "WAIT";
    if (outcome == VN_SERIAL_MODE_FAILED)
        return "FAIL";
    if (outcome == VN_SERIAL_MODE_COMPLETE)
        return "COMPLETE";
    return "READY";
}

static void vn_ui_format_mode_bits(char *text, const char *label,
                                   unsigned long value)
{
    sprintf(text, "%.5s:%04X%04X", label,
            (unsigned int)((value >> 16) & 0xFFFFUL),
            (unsigned int)(value & 0xFFFFUL));
}

static void vn_ui_draw_probe(int in_flight)
{
    const VnSerialProbeStatus *probe;
    VnSerialProbeOperation operation;
    unsigned int value;
    static char text[38];

    probe = vn_serial_probe_status();
    if (in_flight)
    {
        operation = probe->next_operation;
        value = probe->next_value;
        sprintf(text, "PROBE: %02u/%02u IN FLIGHT",
                probe->next_step, probe->total_steps);
        textui_write_field(3, 15, 34, text, TEXTUI_INVERSE);
        sprintf(text, "CALL:  %s",
                vn_serial_probe_operation_text(operation));
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
        if (vn_ui_probe_has_value(operation))
            sprintf(text, "VALUE: $%02X", value & 0xFFU);
        else
            sprintf(text, "VALUE: --");
        textui_write_field(3, 17, 34, text, TEXTUI_NORMAL);
        textui_write_field(3, 19, 34, "WAITING FOR RETURN", TEXTUI_INVERSE);
        textui_write_field(3, 21, 34, "DO NOT START ANOTHER STEP", TEXTUI_NORMAL);
        return;
    }

    if (probe->outcome == VN_SERIAL_PROBE_READY)
    {
        sprintf(text, "PROBE: READY  NEXT %02u/%02u",
                probe->next_step, probe->total_steps);
        textui_write_field(3, 15, 34, text, TEXTUI_NORMAL);
        sprintf(text, "CALL:  %s",
                vn_serial_probe_operation_text(probe->next_operation));
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
        textui_write_field(3, 18, 34, "N RUNS ONE CHECKPOINT", TEXTUI_NORMAL);
        return;
    }

    if (probe->outcome == VN_SERIAL_PROBE_COMPLETE)
        sprintf(text, "PROBE: COMPLETE %02u/%02u",
                probe->total_steps, probe->total_steps);
    else
        sprintf(text, "LAST:  %02u/%02u %s",
                probe->last_step, probe->total_steps,
                vn_ui_probe_outcome_text(probe->outcome));
    textui_write_field(3, 15, 34, text,
                       probe->outcome == VN_SERIAL_PROBE_FAILED ?
                       TEXTUI_INVERSE : TEXTUI_NORMAL);
    sprintf(text, "CALL:  %s",
            vn_serial_probe_operation_text(probe->last_operation));
    textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
    if (vn_ui_probe_has_value(probe->last_operation))
        sprintf(text, "VALUE: $%02X", probe->last_value & 0xFFU);
    else
        sprintf(text, "VALUE: --");
    textui_write_field(3, 17, 34, text, TEXTUI_NORMAL);
    sprintf(text, "A:%04X X:%04X Y:%04X", probe->a, probe->x, probe->y);
    textui_write_field(3, 18, 34, text, TEXTUI_NORMAL);
    sprintf(text, "C:%u ARB:%04X", probe->carry, probe->arbiter_error);
    textui_write_field(3, 19, 34, text, TEXTUI_NORMAL);
    if (probe->next_step > probe->total_steps)
        sprintf(text, "NEXT: COMPLETE");
    else
        sprintf(text, "NEXT: %02u %s", probe->next_step,
                vn_serial_probe_operation_text(probe->next_operation));
    textui_write_field(3, 21, 34, text, TEXTUI_NORMAL);
}

static void vn_ui_draw_serial_io(const VnSerialDiagnosticsDisplay *display,
                                 const VnSerialStats *stats)
{
    const char *status;
    const char *mode;
    const char *restore_status;
    static char text[38];
    static char byte_text[38];
    unsigned int i;
    unsigned int position;

    status = display->io_status;
    if (status == 0 || status[0] == '\0')
        status = "IDLE";
    mode = display->io_mode;
    if (mode == 0 || mode[0] == '\0')
        mode = status;
    restore_status = display->io_restore_status;
    if (restore_status == 0 || restore_status[0] == '\0')
        restore_status = "--";

    sprintf(text, "BYTE IO: %.24s", mode);
    textui_write_field(3, 15, 34, text, TEXTUI_NORMAL);
    if (display->mode_bits_active)
    {
        const VnSerialModeStatus *mode_status;

        mode_status = vn_serial_mode_status();
        if (display->mode_bits_in_flight)
        {
            if (mode_status->stage ==
                VN_SERIAL_MODE_STAGE_INIT)
                sprintf(text, "MODE PINIT IN-FLIGHT");
            else if (mode_status->stage ==
                VN_SERIAL_MODE_STAGE_SETUP_STATUS)
                sprintf(text, "MODE SET STAT %u $%02X IN-FLIGHT",
                        mode_status->setup_index,
                        mode_status->setup_value & 0xFFU);
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_SETUP_WRITE)
                sprintf(text, "MODE SET WRITE %u $%02X IN-FLIGHT",
                        mode_status->setup_index,
                        mode_status->setup_value & 0xFFU);
            else if (mode_status->stage ==
                VN_SERIAL_MODE_STAGE_GET_ORIGINAL)
                sprintf(text, "MODE GET ORIGINAL IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_CLEAR_BIT23)
                sprintf(text, "MODE CLEAR BIT23 IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS)
                sprintf(text, "MODE VERIFY COMMANDS IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_SET_BIT23)
                sprintf(text, "MODE SET BIT23 IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_VERIFY_BIT23)
                sprintf(text, "MODE VERIFY BIT23 IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_RESTORE)
                sprintf(text, "MODE RESTORE ORIGINAL IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_VERIFY_RESTORE)
                sprintf(text, "MODE VERIFY RESTORE IN-FLIGHT");
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS)
                sprintf(text, "MODE PAY STAT %u $%02X IN-FLIGHT",
                        mode_status->payload_index,
                        mode_status->payload_value & 0xFFU);
            else if (mode_status->stage ==
                     VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE)
                sprintf(text, "MODE PAY WRITE %u $%02X IN-FLIGHT",
                        mode_status->payload_index,
                        mode_status->payload_value & 0xFFU);
            else
                sprintf(text, "MODE %.18s IN-FLIGHT",
                        vn_serial_mode_stage_text(mode_status->stage));
        }
        else
        {
            sprintf(text, "STEP:%02u/%02u %.18s",
                    mode_status->step,
                    mode_status->total_steps,
                    vn_serial_mode_stage_text(mode_status->stage));
        }
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);

        sprintf(text, "STATE:%s R:%04X C:%u A:%04X",
                vn_ui_mode_outcome_text(mode_status->outcome),
                mode_status->result_code,
                mode_status->carry,
                mode_status->arbiter_error);
        textui_write_field(3, 17, 34, text, TEXTUI_NORMAL);
        sprintf(text, "CP:%u DISP:%02X ADDR:$%04X B23:%u",
                mode_status->checkpoint,
                mode_status->dispatch_offset & 0xFFU,
                mode_status->dispatch_address,
                mode_status->bit23);
        textui_write_field(3, 18, 34, text, TEXTUI_NORMAL);
        vn_ui_format_mode_bits(text, "ORIG",
                               mode_status->original_mode_bits);
        textui_write_field(3, 19, 34, text, TEXTUI_NORMAL);
        if (mode_status->stage == VN_SERIAL_MODE_STAGE_COMPLETE ||
            mode_status->stage == VN_SERIAL_MODE_STAGE_VERIFY_RESTORE)
            vn_ui_format_mode_bits(text, "REST",
                                   mode_status->restored_mode_bits);
        else
            vn_ui_format_mode_bits(text, "MOD",
                                   mode_status->modified_mode_bits);
        textui_write_field(3, 20, 34, text, TEXTUI_NORMAL);
        if (mode_status->stage == VN_SERIAL_MODE_STAGE_FAILED)
            sprintf(text, "FAIL: %.25s",
                    vn_serial_mode_stage_text(mode_status->failure_stage));
        else if (mode_status->stage == VN_SERIAL_MODE_STAGE_COMPLETE)
            sprintf(text, "LOCAL TX ACCEPTED/MODE RESTORED");
        else if (mode_status->configured_setup &&
                 (mode_status->stage == VN_SERIAL_MODE_STAGE_SETUP_STATUS ||
                  mode_status->stage == VN_SERIAL_MODE_STAGE_SETUP_WRITE))
            sprintf(text, "SET:%u/31 IDX:%u $%02X HOST VERIFY",
                    mode_status->setup_accepted,
                    mode_status->setup_index,
                    mode_status->setup_value & 0xFFU);
        else
            sprintf(text, "PAY:%u/8 IDX:%u $%02X HOST VERIFY",
                    mode_status->payload_accepted,
                    mode_status->payload_index,
                    mode_status->payload_value & 0xFFU);
        textui_write_field(3, 21, 34, text, TEXTUI_NORMAL);
        return;
    }
    if (display->io_byte_count > 0 || display->io_restore_status != 0)
    {
        position = 0;
        sprintf(text, "RESULT: %.26s", status);
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
        sprintf(text, "BYTES:   %04u/%04u",
                display->io_accepted,
                display->io_requested);
        textui_write_field(3, 17, 34, text, TEXTUI_NORMAL);
        sprintf(text, "POLL:%04u ERR:%u/%u/%u",
                display->io_polls,
                stats->parity_errors,
                stats->framing_errors,
                stats->overrun_errors);
        textui_write_field(3, 18, 34, text, TEXTUI_NORMAL);

        sprintf(byte_text, "D0:");
        position = 3;
        for (i = 0; i < display->io_byte_count &&
                    i < 8U; i++)
        {
            sprintf(byte_text + position, " %02X",
                    (unsigned int)display->io_bytes[i]);
            position += 3;
        }
        byte_text[position] = '\0';
        textui_write_field(3, 19, 34, byte_text, TEXTUI_NORMAL);

        sprintf(byte_text, "D8:");
        position = 3;
        for (i = 8U; i < display->io_byte_count &&
                    i < VN_SERIAL_DIAG_DISPLAY_BYTES; i++)
        {
            sprintf(byte_text + position, " %02X",
                    (unsigned int)display->io_bytes[i]);
            position += 3;
        }
        byte_text[position] = '\0';
        textui_write_field(3, 20, 34, byte_text, TEXTUI_NORMAL);

        if (display->io_failure_index != 0xFFFFU)
            sprintf(text, "FAIL %02u $%02X",
                    display->io_failure_index & 0xFFU,
                    display->io_failure_value & 0xFFU);
        else if (display->io_restore_status != 0)
            sprintf(text, "NOTE: %.26s", restore_status);
        else
            sprintf(text, "RESTORE: %.14s VERIFY HOST",
                    restore_status);
        textui_write_field(3, 21, 34, text, TEXTUI_NORMAL);
    }
    else
    {
        sprintf(text, "REQUESTED: %04u", display->io_requested);
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
        sprintf(text, "ACCEPTED:  %04u", display->io_accepted);
        textui_write_field(3, 17, 34, text, TEXTUI_NORMAL);
        sprintf(text, "POLLS:     %04u", display->io_polls);
        textui_write_field(3, 18, 34, text, TEXTUI_NORMAL);
        sprintf(text, "RX:%08lu TX:%08lu",
                stats->rx_bytes - display->rx_baseline,
                stats->tx_bytes - display->tx_baseline);
        textui_write_field(3, 19, 34, text, TEXTUI_NORMAL);
        textui_write_field(3, 21, 34,
                           "P:RX Y:RX16 Z:RX09/17 H:TX8",
                           TEXTUI_NORMAL);
    }
}

void vn_ui_draw_shell(const VnConfig *config,
                      const VnConfigStatus *status,
                      int serial_configured,
                      int serial_backend_enabled)
{
    char detail[34];
    char serial_text[38];
    char title_text[41];

    textui_clear();

    vn_ui_format_title(title_text, VN_APP_NAME, 1);
    textui_write_field(0, 0, 40, title_text, TEXTUI_INVERSE);
    textui_draw_box(0, 1, 40, 22);
    textui_write_field(0, 23, 40,
                       "C:CONFIG D:SERIAL T:TLV ESC/Q:EXIT",
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
    textui_write_field(3, 20, 34, "VINTANET: TLV/PACKET TEST READY", TEXTUI_NORMAL);

    textui_write_field(3, 21, 34, "VERSION " VN_VERSION_TEXT, TEXTUI_NORMAL);

    textui_present();
}

void vn_ui_draw_serial_diagnostics(long baud,
                                   int serial_configured,
                                   int serial_backend_enabled,
                                   const VnSerialDiagnosticsDisplay *display)
{
    const VnSerialStats *stats;
    VnSerialDiagnosticsDisplay default_display;
    static unsigned char rx_history[VN_SERIAL_HISTORY_SIZE];
    static unsigned char tx_history[VN_SERIAL_HISTORY_SIZE];
    static char text[38];
    static char title_text[41];
    unsigned int rx_count;
    unsigned int tx_count;

    if (display == 0)
    {
        default_display.view = VN_SERIAL_DIAG_COUNTERS;
        default_display.probe_in_flight = 0;
        default_display.mode_bits_active = 0;
        default_display.mode_bits_in_flight = 0;
        default_display.io_mode = "";
        default_display.io_status = "";
        default_display.io_requested = 0;
        default_display.io_accepted = 0;
        default_display.io_polls = 0;
        default_display.io_byte_count = 0;
        default_display.io_failure_index = 0xFFFFU;
        default_display.io_failure_value = 0;
        default_display.io_restore_status = "";
        default_display.rx_baseline = 0;
        default_display.tx_baseline = 0;
        display = &default_display;
    }

    stats = vn_serial_stats();
    rx_count = vn_serial_recent_rx(rx_history, VN_SERIAL_HISTORY_SIZE);
    tx_count = vn_serial_recent_tx(tx_history, VN_SERIAL_HISTORY_SIZE);

    textui_clear();
    vn_ui_format_title(title_text, "SERIAL DIAGNOSTICS", 0);
    textui_write_field(0, 0, 40, title_text, TEXTUI_INVERSE);
    textui_draw_box(0, 1, 40, 22);
    textui_write_field(0, 23, 40,
                       "I/W:RAW X:PKT-TX K:PKT-RX C:CLS V:VW",
                       TEXTUI_INVERSE);

    textui_write_field(3, 3, 34, "PORT:   PRN S1 / MODEM S2", TEXTUI_NORMAL);
    sprintf(text, "FORMAT: %ld 8N1", baud);
    textui_write_field(3, 4, 34, text, TEXTUI_NORMAL);
    if (!serial_configured)
        sprintf(text, "STATE:  CONFIG REQUIRED");
    else if (!serial_backend_enabled &&
             vn_serial_status() == VN_SERIAL_STATUS_CLOSED)
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

    if (display->view == VN_SERIAL_DIAG_PROBE)
        vn_ui_draw_probe(display->probe_in_flight);
    else if (display->view == VN_SERIAL_DIAG_IO)
        vn_ui_draw_serial_io(display, stats);
    else
    {
        textui_write_field(3, 15, 34, "RECENT RX:", TEXTUI_NORMAL);
        vn_ui_format_history(text, rx_history, rx_count);
        textui_write_field(3, 16, 34, text, TEXTUI_NORMAL);
        textui_write_field(3, 18, 34, "RECENT TX:", TEXTUI_NORMAL);
        vn_ui_format_history(text, tx_history, tx_count);
        textui_write_field(3, 19, 34, text, TEXTUI_NORMAL);

        sprintf(text, "HIGH WATER: RX %04u TX %04u",
                stats->rx_high_water, stats->tx_high_water);
        textui_write_field(3, 21, 34, text, TEXTUI_NORMAL);
    }
    textui_present();
}
