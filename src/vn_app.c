#include <stdio.h>
#include <string.h>

#include "include/textui.h"
#include "include/textui_input.h"
#include "include/textui_dialog.h"

#include "include/vn_app.h"
#include "include/vn_config.h"
#include "include/vn_config_ui.h"
#include "include/vn_log.h"
#include "include/vn_packet_transport.h"
#include "include/vn_protocol_test.h"
#include "include/vn_serial.h"
#include "include/vn_tlv.h"
#include "include/vn_ui.h"

#define VN_SERIAL_BACKEND_ENABLED 0
#define VN_SERIAL_DIAG_SMOKE_SIZE 8U
#define VN_SERIAL_DIAG_RX16_SIZE 16U
#define VN_SERIAL_DIAG_RX16_PASSES 16U
#define VN_SERIAL_DIAG_RXCMD_SIZE 8U
#define VN_SERIAL_DIAG_RXCMD_PASSES 8U
#define VN_SERIAL_DIAG_PACKET_PASSES 16U
#define VN_SERIAL_DIAG_SWEEP_SIZE 256U
#define VN_SERIAL_DIAG_POLL_PASSES 8U

static const unsigned char vn_app_serial_smoke_pattern[VN_SERIAL_DIAG_SMOKE_SIZE] = {
    0x00U, 0x01U, 0x09U, 0x0AU, 0x0DU, 0x17U, 0x80U, 0xFFU
};

static const unsigned char vn_app_serial_rx16_pattern[VN_SERIAL_DIAG_RX16_SIZE] = {
    0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
    0x08U, 0x09U, 0x0AU, 0x0DU, 0x10U, 0x17U, 0x80U, 0xFFU
};

static const unsigned char vn_app_serial_rxcmd_pattern[VN_SERIAL_DIAG_RXCMD_SIZE] = {
    0x09U, 0x17U, 0x08U, 0x09U, 0x0AU, 0x10U, 0x17U, 0x18U
};

static unsigned char vn_app_serial_sweep[VN_SERIAL_DIAG_SWEEP_SIZE];
static int vn_app_serial_sweep_ready;
static int vn_app_mode8n1_used_this_run;
static char vn_app_log_buffer[128];

static void vn_app_serial_diag_clear_detail(VnSerialDiagnosticsDisplay *display);

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
        sprintf(message, "PASSED %d OF %d TESTS.\nTLV/PACKET OK.",
                result.passed,
                result.total);
        textui_message_dialog("TLV/PACKET TEST", message);
    }
    else
    {
        sprintf(message, "FAILED %d OF %d TESTS.\nFIRST: %.40s",
                result.failed,
                result.total,
                result.first_failed);
        textui_message_dialog("TLV/PACKET TEST", message);
    }
}

static void vn_app_serial_diag_poll(VnSerialDiagnosticsDisplay *display,
                                    unsigned int passes)
{
    unsigned int i;
    unsigned int rx_count;
    unsigned char value;
    char *cursor;

    display->view = VN_SERIAL_DIAG_IO;
    vn_app_serial_diag_clear_detail(display);
    display->io_mode = "RX POLL";
    display->io_requested = 0;
    display->io_accepted = 0;
    sprintf(vn_app_log_buffer, "IO POLL BEGIN passes=%u status=%d",
            passes, (int)vn_serial_status());
    vn_log_line(vn_app_log_buffer);
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        display->io_status = "OPEN FIRST";
        vn_log_line("IO POLL RESULT open_first");
        return;
    }

    rx_count = 0;
    for (i = 0; i < passes; i++)
    {
        vn_serial_poll();
        while (vn_serial_read_byte(&value) > 0)
        {
            if (display->io_byte_count < VN_SERIAL_DIAG_DISPLAY_BYTES)
            {
                display->io_bytes[display->io_byte_count] = value;
                display->io_byte_count++;
            }
            rx_count++;
        }
        display->io_polls++;
        if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
            break;
    }

    if (vn_serial_status() == VN_SERIAL_STATUS_OPEN)
    {
        if (rx_count > 0)
        {
            display->io_status = "RX BYTES";
            if (rx_count > display->io_byte_count)
                display->io_restore_status = "RX FIRST 8";
            else
                display->io_restore_status = "RX CAPTURE";
        }
        else
            display->io_status = "POLL DONE";
    }
    else
        display->io_status = "POLL ERROR";
    display->io_requested = rx_count;
    display->io_accepted = rx_count;
    sprintf(vn_app_log_buffer,
            "IO POLL RESULT polls=%u rx=%u shown=%u status=%d err=%d",
            display->io_polls, rx_count, display->io_byte_count,
            (int)vn_serial_status(), vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
    if (display->io_byte_count > 0)
    {
        cursor = vn_app_log_buffer;
        cursor += sprintf(cursor, "IO POLL RX");
        for (i = 0; i < display->io_byte_count; i++)
            cursor += sprintf(cursor, " %02X",
                              (unsigned int)display->io_bytes[i]);
        vn_log_line(vn_app_log_buffer);
    }
}

static void vn_app_serial_diag_rx_pattern(VnSerialDiagnosticsDisplay *display,
                                          const char *name,
                                          const unsigned char *pattern,
                                          unsigned int length,
                                          unsigned int passes)
{
    unsigned int i;
    unsigned int rx_count;
    unsigned int first_mismatch;
    unsigned int first_extra;
    unsigned char value;
    unsigned char received[VN_SERIAL_DIAG_DISPLAY_BYTES];
    char *cursor;

    display->view = VN_SERIAL_DIAG_IO;
    vn_app_serial_diag_clear_detail(display);
    display->io_mode = name;
    display->io_requested = length;
    display->io_accepted = 0;
    sprintf(vn_app_log_buffer, "%.12s BEGIN status=%d",
            name, (int)vn_serial_status());
    vn_log_line(vn_app_log_buffer);
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        display->io_status = "OPEN FIRST";
        sprintf(vn_app_log_buffer, "%.12s RESULT open_first", name);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    rx_count = 0;
    first_mismatch = 0xFFFFU;
    first_extra = 0xFFFFU;
    for (i = 0; i < passes; i++)
    {
        vn_serial_poll();
        while (vn_serial_read_byte(&value) > 0)
        {
            if (rx_count < VN_SERIAL_DIAG_DISPLAY_BYTES)
                received[rx_count] = value;
            else if (first_extra == 0xFFFFU)
                first_extra = (unsigned int)value;

            if (display->io_byte_count < VN_SERIAL_DIAG_DISPLAY_BYTES)
            {
                display->io_bytes[display->io_byte_count] = value;
                display->io_byte_count++;
            }
            rx_count++;
        }
        display->io_polls++;
        if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
            break;
    }

    display->io_accepted = rx_count;
    for (i = 0; i < rx_count && i < length; i++)
    {
        if (received[i] != pattern[i])
        {
            first_mismatch = i;
            break;
        }
    }

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
        display->io_status = "RX ERROR";
    else if (first_mismatch != 0xFFFFU)
    {
        display->io_status = "RX MISMATCH";
        display->io_failure_index = first_mismatch;
        display->io_failure_value = received[first_mismatch];
    }
    else if (rx_count < length)
    {
        display->io_status = "RX PARTIAL";
        display->io_restore_status = "RX PARTIAL";
    }
    else if (rx_count > length)
    {
        display->io_status = "RX EXTRA";
        display->io_failure_index = length;
        display->io_failure_value = first_extra & 0xFFU;
    }
    else
    {
        display->io_status = "RX PASS";
        display->io_restore_status = "RX MATCH";
    }

    sprintf(vn_app_log_buffer,
            "%.12s RESULT rx=%u shown=%u mismatch=%u extra=%u status=%d err=%d pfo=%u/%u/%u",
            name,
            rx_count, display->io_byte_count, first_mismatch,
            first_extra, (int)vn_serial_status(),
            vn_serial_stats()->last_error,
            vn_serial_stats()->parity_errors,
            vn_serial_stats()->framing_errors,
            vn_serial_stats()->overrun_errors);
    vn_log_line(vn_app_log_buffer);
    if (rx_count > 0)
    {
        cursor = vn_app_log_buffer;
        cursor += sprintf(cursor, "%.12s DATA", name);
        for (i = 0; i < rx_count && i < VN_SERIAL_DIAG_DISPLAY_BYTES; i++)
            cursor += sprintf(cursor, " %02X", (unsigned int)received[i]);
        vn_log_line(vn_app_log_buffer);
    }
}

static void vn_app_serial_diag_rx16(VnSerialDiagnosticsDisplay *display)
{
    vn_app_serial_diag_rx_pattern(display,
                                  "RX16 TEST",
                                  vn_app_serial_rx16_pattern,
                                  VN_SERIAL_DIAG_RX16_SIZE,
                                  VN_SERIAL_DIAG_RX16_PASSES);
}

static void vn_app_serial_diag_rxcmd(VnSerialDiagnosticsDisplay *display)
{
    vn_app_serial_diag_rx_pattern(display,
                                  "RXCMD TEST",
                                  vn_app_serial_rxcmd_pattern,
                                  VN_SERIAL_DIAG_RXCMD_SIZE,
                                  VN_SERIAL_DIAG_RXCMD_PASSES);
}

static void vn_app_serial_diag_copy_bytes(VnSerialDiagnosticsDisplay *display,
                                          const unsigned char *data,
                                          unsigned int length)
{
    unsigned int i;

    display->io_byte_count = 0;
    for (i = 0; i < length && i < VN_SERIAL_DIAG_DISPLAY_BYTES; i++)
    {
        display->io_bytes[i] = data[i];
        display->io_byte_count++;
    }
}

static int vn_app_packet_diag_payload(VnU8 *payload, VnU16 *payload_length)
{
    VnU16 offset;

    offset = 0;
    if (!vn_tlv_add_u16(payload,
                        VN_MAX_PAYLOAD,
                        &offset,
                        VN_TLV_REQUEST_ID,
                        1U))
        return 0;
    *payload_length = offset;
    return 1;
}

static int vn_app_packet_diag_match(const VnPacket *packet,
                                    const VnU8 *payload,
                                    VnU16 payload_length)
{
    if (packet->header.msg_type != VN_MSG_ACK)
        return 0;
    if (packet->header.seq != 1U)
        return 0;
    if (packet->header.flags != VN_FLAG_NONE)
        return 0;
    if (packet->header.payload_len != payload_length)
        return 0;
    if (payload_length > 0 &&
        memcmp(packet->payload, payload, payload_length) != 0)
        return 0;
    return 1;
}

static void vn_app_serial_diag_packet(VnSerialDiagnosticsDisplay *display)
{
    static VnU8 payload[VN_MAX_PAYLOAD];
    static VnPacket packet;
    static unsigned char bytes[VN_SERIAL_DIAG_DISPLAY_BYTES];
    VnU16 payload_length;
    VnU16 packet_length;
    unsigned int accepted;
    unsigned int shown;
    unsigned int i;
    int result;

    display->view = VN_SERIAL_DIAG_IO;
    vn_app_serial_diag_clear_detail(display);
    display->io_mode = "PKT ACK";
    display->io_requested = 0;
    display->io_accepted = 0;
    display->io_polls = 0;
    vn_log_line("PKT DIAG BEGIN");

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        display->io_status = "OPEN FIRST";
        vn_log_line("PKT DIAG RESULT open_first");
        return;
    }

    if (!vn_app_packet_diag_payload(payload, &payload_length))
    {
        display->io_status = "PAYLOAD FAIL";
        vn_log_line("PKT DIAG RESULT payload_fail");
        return;
    }

    vn_packet_transport_reset();
    result = vn_packet_transport_send(VN_MSG_ACK,
                                      payload,
                                      payload_length,
                                      1U,
                                      VN_FLAG_NONE,
                                      &packet_length,
                                      &accepted);
    display->io_requested = packet_length;
    display->io_accepted = accepted;
    shown = vn_packet_transport_last_tx(bytes,
                                        VN_SERIAL_DIAG_DISPLAY_BYTES);
    vn_app_serial_diag_copy_bytes(display, bytes, shown);

    if (result != VN_ERR_NONE)
    {
        display->io_status = "PKT TX ERROR";
        display->io_failure_index = result & 0xFFFFU;
        sprintf(vn_app_log_buffer,
                "PKT DIAG RESULT tx_error result=%d accepted=%u len=%u",
                result, accepted, packet_length);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    result = 0;
    for (i = 0; i < VN_SERIAL_DIAG_PACKET_PASSES; i++)
    {
        result = vn_packet_transport_poll(&packet);
        display->io_polls++;
        if (result != 0)
            break;
    }

    shown = vn_packet_transport_last_rx(bytes,
                                        VN_SERIAL_DIAG_DISPLAY_BYTES);
    if (shown > 0)
        vn_app_serial_diag_copy_bytes(display, bytes, shown);

    if (result == 1)
    {
        if (vn_app_packet_diag_match(&packet, payload, payload_length))
        {
            display->io_status = "PKT RX PASS";
            display->io_restore_status = "ACK MATCH";
        }
        else
        {
            display->io_status = "PKT MISMATCH";
            display->io_failure_index = packet.header.msg_type;
            display->io_failure_value = packet.header.seq;
        }
    }
    else if (result < 0)
    {
        display->io_status = "PKT RX ERROR";
        display->io_failure_index = (unsigned int)(-result);
    }
    else
    {
        display->io_status = "PKT TX QUEUED";
        display->io_restore_status = "WAIT PEER";
    }

    sprintf(vn_app_log_buffer,
            "PKT DIAG RESULT result=%d tx=%u/%u polls=%u rxbuf=%d status=%d",
            result,
            accepted,
            packet_length,
            display->io_polls,
            vn_packet_transport_rx_buffered(),
            (int)vn_serial_status());
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_clear_detail(VnSerialDiagnosticsDisplay *display)
{
    unsigned int i;

    display->io_byte_count = 0;
    display->io_mode = 0;
    display->mode_bits_active = 0;
    display->mode_bits_in_flight = 0;
    display->io_failure_index = 0xFFFFU;
    display->io_failure_value = 0;
    display->io_restore_status = 0;
    for (i = 0; i < VN_SERIAL_DIAG_DISPLAY_BYTES; i++)
        display->io_bytes[i] = 0;
}

static void vn_app_serial_diag_init(VnSerialDiagnosticsDisplay *display)
{
    memset(display, 0, sizeof(*display));
    display->view = VN_SERIAL_DIAG_COUNTERS;
    display->io_status = "IDLE";
    vn_app_serial_diag_clear_detail(display);
}

static void vn_app_serial_diag_baseline(VnSerialDiagnosticsDisplay *display)
{
    const VnSerialStats *stats;

    stats = vn_serial_stats();
    display->rx_baseline = stats->rx_bytes;
    display->tx_baseline = stats->tx_bytes;
}

static void vn_app_serial_diag_draw(const VnConfig *config,
                                    int serial_configured,
                                    const VnSerialDiagnosticsDisplay *display)
{
    vn_ui_draw_serial_diagnostics(config->baud, serial_configured,
                                  VN_SERIAL_BACKEND_ENABLED, display);
}

static int vn_app_serial_diag_open(const VnConfig *config,
                                   int serial_configured,
                                   VnSerialDiagnosticsDisplay *display)
{
    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = 0;
    display->io_accepted = 0;
    display->io_polls = 0;
    vn_app_serial_diag_clear_detail(display);
    sprintf(vn_app_log_buffer, "IO OPEN BEGIN configured=%d baud=%ld",
            serial_configured, config->baud);
    vn_log_line(vn_app_log_buffer);
    if (vn_app_mode8n1_used_this_run)
    {
        display->io_status = "I BLOCKED AFTER H";
        vn_log_line("IO OPEN BLOCKED after_mode8n1");
        return 0;
    }
    if (!serial_configured)
    {
        display->io_status = "CONFIG REQUIRED";
        vn_log_line("IO OPEN RESULT config_required");
        return 0;
    }

    display->io_status = "OPEN PINIT FLIGHT";
    vn_app_serial_diag_draw(config, serial_configured, display);
    if (vn_serial_open(1, config->baud))
    {
        display->io_status = "OPEN";
        vn_app_serial_diag_baseline(display);
        sprintf(vn_app_log_buffer, "IO OPEN RESULT ok=1 status=%d err=%d",
                (int)vn_serial_status(), vn_serial_stats()->last_error);
        vn_log_line(vn_app_log_buffer);
        return 1;
    }

    display->io_status = "OPEN FAILED";
    vn_app_serial_diag_baseline(display);
    sprintf(vn_app_log_buffer, "IO OPEN RESULT ok=0 status=%d err=%d",
            (int)vn_serial_status(), vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
    return 0;
}

static void vn_app_serial_diag_close(VnSerialDiagnosticsDisplay *display)
{
    vn_log_line("IO CLOSE BEGIN");
    vn_serial_close();
    display->view = VN_SERIAL_DIAG_IO;
    display->io_status = "CLOSED";
    display->io_requested = 0;
    display->io_accepted = 0;
    display->io_polls = 0;
    vn_app_serial_diag_clear_detail(display);
    vn_app_serial_diag_baseline(display);
    sprintf(vn_app_log_buffer, "IO CLOSE RESULT status=%d err=%d",
            (int)vn_serial_status(), vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_send(VnSerialDiagnosticsDisplay *display,
                                    const unsigned char *data,
                                    unsigned int length,
                                    const char *name)
{
    unsigned int accepted;

    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = length;
    display->io_accepted = 0;
    vn_app_serial_diag_clear_detail(display);
    sprintf(vn_app_log_buffer, "IO QUEUE BEGIN name=%.20s len=%u status=%d",
            name, length, (int)vn_serial_status());
    vn_log_line(vn_app_log_buffer);
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        display->io_status = "OPEN FIRST";
        vn_log_line("IO QUEUE RESULT open_first");
        return;
    }

    accepted = vn_serial_write(data, length);
    display->io_accepted = accepted;
    if (accepted == length)
        display->io_status = name;
    else
        display->io_status = "PARTIAL QUEUE";

    vn_app_serial_diag_poll(display, VN_SERIAL_DIAG_POLL_PASSES);
    if (vn_serial_status() == VN_SERIAL_STATUS_OPEN && accepted == length)
        display->io_status = name;
    sprintf(vn_app_log_buffer,
            "IO QUEUE RESULT name=%.20s accepted=%u status=%d err=%d",
            name, accepted, (int)vn_serial_status(),
            vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_send_smoke(VnSerialDiagnosticsDisplay *display)
{
    vn_app_serial_diag_send(display,
                            vn_app_serial_smoke_pattern,
                            VN_SERIAL_DIAG_SMOKE_SIZE,
                            "SMOKE QUEUED");
}

static void vn_app_serial_diag_send_raw_smoke(VnSerialDiagnosticsDisplay *display)
{
    unsigned int written;

    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = VN_SERIAL_DIAG_SMOKE_SIZE;
    display->io_accepted = 0;
    vn_app_serial_diag_clear_detail(display);
    sprintf(vn_app_log_buffer, "RAW W BEGIN len=%u status=%d",
            VN_SERIAL_DIAG_SMOKE_SIZE, (int)vn_serial_status());
    vn_log_line(vn_app_log_buffer);
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        display->io_status = "OPEN FIRST";
        vn_log_line("RAW W RESULT open_first");
        return;
    }

    written = vn_serial_diag_write_raw(vn_app_serial_smoke_pattern,
                                       VN_SERIAL_DIAG_SMOKE_SIZE);
    display->io_accepted = written;
    if (written == VN_SERIAL_DIAG_SMOKE_SIZE)
        display->io_status = "RAW SMOKE";
    else if (vn_serial_status() == VN_SERIAL_STATUS_OPEN)
        display->io_status = "RAW PARTIAL";
    else
        display->io_status = "RAW ERROR";
    sprintf(vn_app_log_buffer,
            "RAW W RESULT accepted=%u status=%d err=%d",
            written, (int)vn_serial_status(),
            vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_send_unconfigured_raw_smoke(
    VnSerialDiagnosticsDisplay *display)
{
    unsigned int written;

    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = VN_SERIAL_DIAG_SMOKE_SIZE;
    display->io_accepted = 0;
    display->io_polls = 0;
    vn_app_serial_diag_clear_detail(display);
    sprintf(vn_app_log_buffer, "UNCFG RAW BEGIN len=%u",
            VN_SERIAL_DIAG_SMOKE_SIZE);
    vn_log_line(vn_app_log_buffer);

    written = vn_serial_diag_write_raw_printer_unconfigured(
        vn_app_serial_smoke_pattern,
        VN_SERIAL_DIAG_SMOKE_SIZE);
    display->io_accepted = written;
    if (written == VN_SERIAL_DIAG_SMOKE_SIZE)
        display->io_status = "UNCFG RAW";
    else if (written > 0)
        display->io_status = "UNCFG PARTIAL";
    else
        display->io_status = "UNCFG ERROR";
    sprintf(vn_app_log_buffer,
            "UNCFG RAW RESULT accepted=%u status=%d err=%d",
            written, (int)vn_serial_status(),
            vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_printer_next(
    const VnConfig *config,
    int serial_configured,
    VnSerialDiagnosticsDisplay *display,
    unsigned int *step,
    unsigned int *raw_written)
{
    static char status_text[32];
    unsigned int total_steps;
    unsigned int raw_step;
    unsigned int raw_index;
    unsigned char value;
    int result;

    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = VN_SERIAL_DIAG_SMOKE_SIZE;
    display->io_accepted = *raw_written;
    display->io_polls = 0;
    vn_app_serial_diag_clear_detail(display);

    total_steps = 1U + (VN_SERIAL_DIAG_SMOKE_SIZE * 2U);
    if (*step == 0 || *step > total_steps)
    {
        *step = 1U;
        *raw_written = 0;
        display->io_accepted = 0;
    }
    display->io_polls = *step - 1U;

    if (*step == 1U)
    {
        display->io_status = "L01 OPEN FLIGHT";
        vn_app_serial_diag_draw(config, serial_configured, display);
        sprintf(vn_app_log_buffer, "LANE L OPEN BEGIN baud=%ld",
                config->baud);
        vn_log_line(vn_app_log_buffer);

        result = vn_serial_open(1, config->baud);
        if (result)
        {
            (*step)++;
            display->io_status = "L OPEN PASS";
        }
        else
            display->io_status = "L OPEN FAIL";
        sprintf(vn_app_log_buffer,
                "LANE L OPEN RESULT result=%d status=%d err=%d",
                result, (int)vn_serial_status(),
                vn_serial_stats()->last_error);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    raw_step = *step - 2U;
    raw_index = raw_step / 2U;
    if (raw_index >= VN_SERIAL_DIAG_SMOKE_SIZE)
    {
        display->io_status = "LANE A DONE";
        return;
    }

    if ((raw_step % 2U) == 0)
    {
        sprintf(status_text, "L%02u STAT RAW%u", *step, raw_index);
        display->io_status = status_text;
        vn_app_serial_diag_draw(config, serial_configured, display);
        sprintf(vn_app_log_buffer,
                "LANE L STATUS BEGIN step=%u index=%u",
                *step, raw_index);
        vn_log_line(vn_app_log_buffer);

        result = vn_serial_diag_printer_status_tx();
        if (result > 0)
        {
            (*step)++;
            display->io_status = "L TX READY";
        }
        else if (result == 0)
            display->io_status = "L TX WAIT";
        else
            display->io_status = "L TX ERROR";
        sprintf(vn_app_log_buffer,
                "LANE L STATUS RESULT step=%u index=%u result=%d err=%d",
                *step, raw_index, result, vn_serial_stats()->last_error);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    value = vn_app_serial_smoke_pattern[raw_index];
    sprintf(status_text, "L%02u RAW $%02X", *step, (unsigned int)value);
    display->io_status = status_text;
    vn_app_serial_diag_draw(config, serial_configured, display);
    sprintf(vn_app_log_buffer,
            "LANE L WRITE BEGIN step=%u index=%u value=%02X",
            *step, raw_index, (unsigned int)value);
    vn_log_line(vn_app_log_buffer);

    result = vn_serial_diag_printer_write_byte(value);
    if (result)
    {
        (*raw_written)++;
        display->io_accepted = *raw_written;
        (*step)++;
        if (*raw_written == VN_SERIAL_DIAG_SMOKE_SIZE)
            display->io_status = "LANE A DONE";
        else
            display->io_status = "L RAW PASS";
    }
    else
        display->io_status = "L RAW FAIL";
    sprintf(vn_app_log_buffer,
            "LANE L WRITE RESULT step=%u index=%u value=%02X result=%d accepted=%u err=%d",
            *step, raw_index, (unsigned int)value, result,
            *raw_written, vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_modem_next(
    const VnConfig *config,
    int serial_configured,
    VnSerialDiagnosticsDisplay *display,
    unsigned int *step,
    unsigned int *raw_written)
{
    static char status_text[32];
    unsigned int total_steps;
    unsigned int raw_step;
    unsigned int raw_index;
    unsigned char value;
    int result;

    display->view = VN_SERIAL_DIAG_IO;
    display->io_requested = VN_SERIAL_DIAG_SMOKE_SIZE;
    display->io_accepted = *raw_written;
    display->io_polls = 0;
    vn_app_serial_diag_clear_detail(display);

    total_steps = 1U + (VN_SERIAL_DIAG_SMOKE_SIZE * 2U);
    if (*step == 0 || *step > total_steps)
    {
        *step = 1U;
        *raw_written = 0;
        display->io_accepted = 0;
    }
    display->io_polls = *step - 1U;

    if (*step == 1U)
    {
        sprintf(status_text, "M%02u PINIT FLIGHT", *step);
        display->io_status = status_text;
        vn_app_serial_diag_draw(config, serial_configured, display);
        vn_log_line("LANE M PINIT BEGIN");

        result = vn_serial_diag_modem_init();
        if (result)
        {
            (*step)++;
            display->io_status = "M PINIT PASS";
        }
        else
            display->io_status = "M PINIT FAIL";
        sprintf(vn_app_log_buffer, "LANE M PINIT RESULT result=%d err=%d",
                result, vn_serial_stats()->last_error);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    raw_step = *step - 2U;
    raw_index = raw_step / 2U;
    if (raw_index >= VN_SERIAL_DIAG_SMOKE_SIZE)
    {
        display->io_status = "MOD DONE";
        return;
    }

    if ((raw_step % 2U) == 0)
    {
        sprintf(status_text, "M%02u STAT RAW%u", *step, raw_index);
        display->io_status = status_text;
        vn_app_serial_diag_draw(config, serial_configured, display);
        sprintf(vn_app_log_buffer,
                "LANE M STATUS BEGIN step=%u index=%u",
                *step, raw_index);
        vn_log_line(vn_app_log_buffer);

        result = vn_serial_diag_modem_status_tx();
        if (result > 0)
        {
            (*step)++;
            display->io_status = "M TX READY";
        }
        else if (result == 0)
            display->io_status = "M TX WAIT";
        else
            display->io_status = "M TX ERROR";
        sprintf(vn_app_log_buffer,
                "LANE M STATUS RESULT step=%u index=%u result=%d err=%d",
                *step, raw_index, result, vn_serial_stats()->last_error);
        vn_log_line(vn_app_log_buffer);
        return;
    }

    value = vn_app_serial_smoke_pattern[raw_index];
    sprintf(status_text, "M%02u RAW $%02X", *step, (unsigned int)value);
    display->io_status = status_text;
    vn_app_serial_diag_draw(config, serial_configured, display);
    sprintf(vn_app_log_buffer,
            "LANE M WRITE BEGIN step=%u index=%u value=%02X",
            *step, raw_index, (unsigned int)value);
    vn_log_line(vn_app_log_buffer);

    result = vn_serial_diag_modem_write_byte(value);
    if (result)
    {
        (*raw_written)++;
        display->io_accepted = *raw_written;
        (*step)++;
        if (*raw_written == VN_SERIAL_DIAG_SMOKE_SIZE)
            display->io_status = "MOD DONE";
        else
            display->io_status = "M RAW PASS";
    }
    else
        display->io_status = "M RAW FAIL";
    sprintf(vn_app_log_buffer,
            "LANE M WRITE RESULT step=%u index=%u value=%02X result=%d accepted=%u err=%d",
            *step, raw_index, (unsigned int)value, result,
            *raw_written, vn_serial_stats()->last_error);
    vn_log_line(vn_app_log_buffer);
}

static int vn_app_serial_mode_stage_enters_firmware(VnSerialModeStage stage)
{
    return stage == VN_SERIAL_MODE_STAGE_INIT ||
           stage == VN_SERIAL_MODE_STAGE_CLEAR_BIT23 ||
           stage == VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS ||
           stage == VN_SERIAL_MODE_STAGE_SETUP_STATUS ||
           stage == VN_SERIAL_MODE_STAGE_SETUP_WRITE ||
           stage == VN_SERIAL_MODE_STAGE_GET_ORIGINAL ||
           stage == VN_SERIAL_MODE_STAGE_SET_BIT23 ||
           stage == VN_SERIAL_MODE_STAGE_VERIFY_BIT23 ||
           stage == VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS ||
           stage == VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE ||
           stage == VN_SERIAL_MODE_STAGE_RESTORE ||
           stage == VN_SERIAL_MODE_STAGE_VERIFY_RESTORE;
}

static int vn_app_serial_mode_stage_is_chunk(VnSerialModeStage stage)
{
    return stage == VN_SERIAL_MODE_STAGE_SETUP_STATUS ||
           stage == VN_SERIAL_MODE_STAGE_SETUP_WRITE ||
           stage == VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS ||
           stage == VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE;
}

static void vn_app_serial_diag_mode_show(VnSerialDiagnosticsDisplay *display)
{
    const VnSerialModeStatus *status;

    status = vn_serial_mode_status();
    display->view = VN_SERIAL_DIAG_IO;
    display->mode_bits_active = 1;
    if (status->configured_setup)
        display->io_mode = "LANE A 8N1 MODE";
    else
        display->io_mode = "LANE A MODE BITS";
    display->io_status = vn_serial_mode_stage_text(status->stage);
    display->io_requested = VN_SERIAL_DIAG_SMOKE_SIZE;
    display->io_accepted = status->payload_accepted;
    display->io_polls = status->step;
}

static void vn_app_serial_diag_mode_next(
    const VnConfig *config,
    int serial_configured,
    VnSerialDiagnosticsDisplay *display)
{
    const VnSerialModeStatus *status;
    int in_flight;
    int result;

    vn_app_serial_diag_clear_detail(display);
    vn_app_serial_diag_mode_show(display);
    status = vn_serial_mode_status();
    in_flight = vn_app_serial_mode_stage_enters_firmware(status->stage);
    if (in_flight)
    {
        display->mode_bits_in_flight = 1;
        vn_app_serial_diag_draw(config, serial_configured, display);
    }
    sprintf(vn_app_log_buffer,
            "MODE NEXT BEGIN step=%u stage=%u",
            status->step, (unsigned int)status->stage);
    vn_log_line(vn_app_log_buffer);

    result = vn_serial_mode_next();
    display->mode_bits_in_flight = 0;
    vn_app_serial_diag_mode_show(display);
    status = vn_serial_mode_status();
    sprintf(vn_app_log_buffer,
            "MODE NEXT RESULT result=%d step=%u stage=%u outcome=%u code=%04X image=%04X%04X accepted=%u",
            result,
            status->step,
            (unsigned int)status->stage,
            (unsigned int)status->outcome,
            status->result_code,
            (unsigned int)((status->returned_mode_bits >> 16) & 0xFFFFUL),
            (unsigned int)(status->returned_mode_bits & 0xFFFFUL),
            status->payload_accepted);
    vn_log_line(vn_app_log_buffer);
    sprintf(vn_app_log_buffer,
            "MODE SETUP accepted=%u index=%u value=%02X configured=%u",
            status->setup_accepted,
            status->setup_index,
            status->setup_value & 0xFFU,
            status->configured_setup);
    vn_log_line(vn_app_log_buffer);
    sprintf(vn_app_log_buffer,
            "MODE EXT cp=%u off=%02X addr=%04X ptr=%02X/%02X/%02X sp=%04X ps=%04X db=%02X dp=%04X",
            status->checkpoint,
            status->dispatch_offset & 0xFFU,
            status->dispatch_address,
            status->pointer_a & 0xFFU,
            status->pointer_x & 0xFFU,
            status->pointer_y & 0xFFU,
            status->saved_stack,
            status->processor_status,
            status->data_bank & 0xFFU,
            status->direct_page);
    vn_log_line(vn_app_log_buffer);
    sprintf(vn_app_log_buffer,
            "MODE CMD %04X %04X %04X %04X",
            status->command_word0,
            status->command_word1,
            status->command_word2,
            status->command_word3);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_mode_chunk(
    const VnConfig *config,
    int serial_configured,
    VnSerialDiagnosticsDisplay *display)
{
    unsigned int budget;
    const VnSerialModeStatus *status;

    budget = 80U;
    while (budget > 0U)
    {
        status = vn_serial_mode_status();
        if (!vn_app_serial_mode_stage_is_chunk(status->stage))
            break;

        vn_app_serial_diag_mode_next(config, serial_configured, display);
        status = vn_serial_mode_status();
        if (status->outcome == VN_SERIAL_MODE_WAIT ||
            status->outcome == VN_SERIAL_MODE_FAILED ||
            status->outcome == VN_SERIAL_MODE_COMPLETE)
            break;
        if (!vn_app_serial_mode_stage_is_chunk(status->stage))
            break;
        budget--;
    }

    if (budget == 0U)
    {
        display->io_status = "MODE CHUNK LIMIT";
        vn_log_line("MODE CHUNK LIMIT");
    }
}

static void vn_app_serial_diag_mode_restore(
    const VnConfig *config,
    int serial_configured,
    VnSerialDiagnosticsDisplay *display)
{
    const VnSerialModeStatus *status;
    int result;

    vn_app_serial_diag_clear_detail(display);
    vn_serial_mode_prepare_restore();
    vn_app_serial_diag_mode_show(display);
    status = vn_serial_mode_status();
    if (vn_app_serial_mode_stage_enters_firmware(status->stage))
    {
        display->mode_bits_in_flight = 1;
        vn_app_serial_diag_draw(config, serial_configured, display);
    }
    sprintf(vn_app_log_buffer,
            "MODE RESTORE BEGIN step=%u stage=%u",
            status->step, (unsigned int)status->stage);
    vn_log_line(vn_app_log_buffer);

    result = vn_serial_mode_restore_next();
    display->mode_bits_in_flight = 0;
    vn_app_serial_diag_mode_show(display);
    status = vn_serial_mode_status();
    sprintf(vn_app_log_buffer,
            "MODE RESTORE RESULT result=%d step=%u stage=%u outcome=%u code=%04X image=%04X%04X",
            result,
            status->step,
            (unsigned int)status->stage,
            (unsigned int)status->outcome,
            status->result_code,
            (unsigned int)((status->returned_mode_bits >> 16) & 0xFFFFUL),
            (unsigned int)(status->returned_mode_bits & 0xFFFFUL));
    vn_log_line(vn_app_log_buffer);
    sprintf(vn_app_log_buffer,
            "MODE RESTORE EXT cp=%u off=%02X addr=%04X ptr=%02X/%02X/%02X sp=%04X cmd=%04X %04X %04X %04X",
            status->checkpoint,
            status->dispatch_offset & 0xFFU,
            status->dispatch_address,
            status->pointer_a & 0xFFU,
            status->pointer_x & 0xFFU,
            status->pointer_y & 0xFFU,
            status->saved_stack,
            status->command_word0,
            status->command_word1,
            status->command_word2,
            status->command_word3);
    vn_log_line(vn_app_log_buffer);
}

static void vn_app_serial_diag_send_sweep(VnSerialDiagnosticsDisplay *display)
{
    unsigned int i;

    if (!vn_app_serial_sweep_ready)
    {
        for (i = 0; i < VN_SERIAL_DIAG_SWEEP_SIZE; i++)
            vn_app_serial_sweep[i] = (unsigned char)i;
        vn_app_serial_sweep_ready = 1;
    }

    vn_app_serial_diag_send(display,
                            vn_app_serial_sweep,
                            VN_SERIAL_DIAG_SWEEP_SIZE,
                            "SWEEP QUEUED");
}

static void vn_app_serial_diag_next_view(VnSerialDiagnosticsDisplay *display)
{
    if (display->view == VN_SERIAL_DIAG_COUNTERS)
        display->view = VN_SERIAL_DIAG_PROBE;
    else if (display->view == VN_SERIAL_DIAG_PROBE)
        display->view = VN_SERIAL_DIAG_IO;
    else
        display->view = VN_SERIAL_DIAG_COUNTERS;
}

static void vn_app_run_serial_diagnostics(const VnConfig *config,
                                          int serial_configured)
{
    TextUiKeyEvent event;
    VnSerialDiagnosticsDisplay display;
    int diagnostics_opened;
    unsigned int printer_step;
    unsigned int printer_raw_written;
    unsigned int modem_step;
    unsigned int modem_raw_written;

    vn_app_serial_diag_init(&display);
    vn_app_serial_diag_baseline(&display);
    vn_serial_mode_reset();
    vn_log_line("SERIAL DIAG ENTER");
    diagnostics_opened = 0;
    printer_step = 1U;
    printer_raw_written = 0;
    modem_step = 1U;
    modem_raw_written = 0;
    vn_app_serial_diag_draw(config, serial_configured, &display);

    while (1)
    {
        if (textui_poll_key_event(&event))
        {
            if (event.key == TEXTUI_KEY_ESCAPE ||
                event.ch == 'Q' || event.ch == 'q')
                break;
            if (event.ch == 'V' || event.ch == 'v')
            {
                vn_app_serial_diag_next_view(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'R' || event.ch == 'r')
            {
                vn_serial_probe_reset();
                vn_serial_mode_reset();
                vn_log_line("SERIAL DIAG RESET");
                printer_step = 1U;
                printer_raw_written = 0;
                modem_step = 1U;
                modem_raw_written = 0;
                display.view = VN_SERIAL_DIAG_PROBE;
                display.probe_in_flight = 0;
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'N' || event.ch == 'n')
            {
                display.view = VN_SERIAL_DIAG_PROBE;
                display.probe_in_flight = 1;
                vn_app_serial_diag_draw(config, serial_configured, &display);
                vn_log_line("PROBE NEXT BEGIN");
                vn_serial_probe_next();
                display.probe_in_flight = 0;
                sprintf(vn_app_log_buffer,
                        "PROBE NEXT RESULT step=%u outcome=%u a=%04X x=%04X y=%04X c=%u arb=%04X",
                        vn_serial_probe_status()->last_step,
                        (unsigned int)vn_serial_probe_status()->outcome,
                        vn_serial_probe_status()->a,
                        vn_serial_probe_status()->x,
                        vn_serial_probe_status()->y,
                        vn_serial_probe_status()->carry,
                        vn_serial_probe_status()->arbiter_error);
                vn_log_line(vn_app_log_buffer);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'I' || event.ch == 'i')
            {
                diagnostics_opened =
                    vn_app_serial_diag_open(config, serial_configured,
                                            &display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'S' || event.ch == 's')
            {
                vn_app_serial_diag_send_smoke(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'W' || event.ch == 'w')
            {
                vn_app_serial_diag_send_raw_smoke(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'U' || event.ch == 'u')
            {
                vn_app_serial_diag_send_unconfigured_raw_smoke(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'L' || event.ch == 'l')
            {
                vn_app_serial_diag_printer_next(config, serial_configured,
                                                &display, &printer_step,
                                                &printer_raw_written);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'M' || event.ch == 'm')
            {
                vn_app_serial_diag_modem_next(config, serial_configured,
                                              &display, &modem_step,
                                              &modem_raw_written);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'B' || event.ch == 'b')
            {
                if (vn_serial_mode_status()->configured_setup)
                    vn_serial_mode_reset();
                vn_app_serial_diag_mode_next(config, serial_configured,
                                             &display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'H' || event.ch == 'h')
            {
                const VnSerialModeStatus *mode_status;

                vn_app_mode8n1_used_this_run = 1;
                mode_status = vn_serial_mode_status();
                if (!mode_status->configured_setup ||
                    mode_status->stage == VN_SERIAL_MODE_STAGE_COMPLETE ||
                    mode_status->stage == VN_SERIAL_MODE_STAGE_FAILED)
                    vn_serial_mode8n1_reset();
                mode_status = vn_serial_mode_status();
                if (vn_app_serial_mode_stage_is_chunk(mode_status->stage))
                    vn_app_serial_diag_mode_chunk(config, serial_configured,
                                                  &display);
                else
                    vn_app_serial_diag_mode_next(config, serial_configured,
                                                 &display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'O' || event.ch == 'o')
            {
                vn_app_serial_diag_mode_restore(config, serial_configured,
                                                &display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'A' || event.ch == 'a')
            {
                vn_app_serial_diag_send_sweep(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'P' || event.ch == 'p')
            {
                vn_app_serial_diag_poll(&display,
                                        VN_SERIAL_DIAG_POLL_PASSES);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'Y' || event.ch == 'y')
            {
                vn_app_serial_diag_rx16(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'Z' || event.ch == 'z')
            {
                vn_app_serial_diag_rxcmd(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'K' || event.ch == 'k')
            {
                vn_app_serial_diag_packet(&display);
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
            else if (event.ch == 'C' || event.ch == 'c')
            {
                if (diagnostics_opened)
                {
                    vn_app_serial_diag_close(&display);
                    diagnostics_opened = 0;
                    printer_step = 1U;
                    printer_raw_written = 0;
                    modem_step = 1U;
                    modem_raw_written = 0;
                }
                else
                {
                    display.view = VN_SERIAL_DIAG_IO;
                    display.io_status = "NOT OPENED HERE";
                    display.io_requested = 0;
                    display.io_accepted = 0;
                    display.io_polls = 0;
                    vn_app_serial_diag_clear_detail(&display);
                    printer_step = 1U;
                    printer_raw_written = 0;
                    modem_step = 1U;
                    modem_raw_written = 0;
                }
                vn_app_serial_diag_draw(config, serial_configured, &display);
            }
        }
    }

    if (diagnostics_opened)
    {
        vn_log_line("SERIAL DIAG EXIT CLOSE");
        vn_serial_close();
    }
    vn_log_line("SERIAL DIAG EXIT");
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
    vn_log_start();
    vn_log_line("APP START");
    vn_config_load(VN_CONFIG_DEFAULT_FILE, &config, &status);
    serial_configured = vn_app_port_one_selected(&config);
    sprintf(vn_app_log_buffer,
            "CONFIG result=%d ports=%.20s baud=%ld configured=%d",
            (int)status.result, config.ports, config.baud,
            serial_configured);
    vn_log_line(vn_app_log_buffer);
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
    vn_log_line("APP EXIT");
    textui_restore();
    return 0;
}
