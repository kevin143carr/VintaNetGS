#include <stdio.h>
#include <string.h>

#include "include/vn_serial.h"
#include "include/vn_serial_fw.h"

#define VN_SERIAL_SLOT_PRINTER 1
#define VN_SERIAL_SLOT_MODEM 2
#define VN_SERIAL_POLL_BUDGET 32U
#define VN_SERIAL_CLOSE_RESTORE_ENABLED 0
#define VN_SERIAL_MODE_BIT23 0x00800000UL
#define VN_SERIAL_MODE_TOTAL_STEPS 23U
#define VN_SERIAL_MODE_8N1_TOTAL_STEPS 88U

#define VN_FW_COMMAND 0x09U
#define VN_FW_ALT_COMMAND 0x17U

#define VN_FW_ERROR_PARITY 0x01U
#define VN_FW_ERROR_FRAMING 0x02U
#define VN_FW_ERROR_OVERRUN 0x04U

typedef struct VnSerialRing {
    unsigned char *data;
    unsigned int size;
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    unsigned int high_water;
} VnSerialRing;

static unsigned char vn_serial_rx_storage[VN_SERIAL_RX_BUFFER_SIZE];
static unsigned char vn_serial_tx_storage[VN_SERIAL_TX_BUFFER_SIZE];
static unsigned char vn_serial_test_storage[17];
static unsigned char vn_serial_rx_history[VN_SERIAL_HISTORY_SIZE];
static unsigned char vn_serial_tx_history[VN_SERIAL_HISTORY_SIZE];

static VnSerialRing vn_serial_rx_ring;
static VnSerialRing vn_serial_tx_ring;
static VnSerialStats vn_serial_current_stats;
static VnSerialStatus vn_serial_current_status = VN_SERIAL_STATUS_CLOSED;
static unsigned int vn_serial_rx_history_count;
static unsigned int vn_serial_rx_history_next;
static unsigned int vn_serial_tx_history_count;
static unsigned int vn_serial_tx_history_next;
static unsigned int vn_serial_escape_index;
static char vn_serial_status_buffer[32];
static VnSerialProbeStatus vn_serial_probe_current;
static VnSerialModeStatus vn_serial_mode_current;
static int vn_serial_probe_initialized;
static int vn_serial_mode_initialized;

static int vn_serial_configure_slot(unsigned int slot, long baud);

static const unsigned char vn_serial_escape_bytes[] = {
    VN_FW_COMMAND,
    VN_FW_ALT_COMMAND,
    VN_FW_COMMAND,
    VN_FW_ALT_COMMAND,
    VN_FW_COMMAND
};

static const unsigned char vn_serial_probe_write_bytes[] = {
    0x09U, 0x31U, 0x30U, 0x42U,
    0x09U, 0x30U, 0x44U,
    0x09U, 0x30U, 0x50U,
    0x09U, 0x43U, 0x44U,
    0x09U, 0x58U, 0x44U,
    0x09U, 0x46U, 0x44U,
    0x09U, 0x4CU, 0x44U,
    0x09U, 0x45U, 0x44U,
    0x09U, 0x4DU, 0x44U,
    0x09U, 0x42U, 0x45U
};

static const unsigned char vn_serial_mode_payload[] = {
    0x00U, 0x01U, 0x09U, 0x0AU,
    0x0DU, 0x17U, 0x80U, 0xFFU
};

static void vn_serial_probe_describe(unsigned int step,
                                     VnSerialProbeOperation *operation,
                                     unsigned int *value)
{
    unsigned int write_index;

    *operation = VN_SERIAL_PROBE_NONE;
    *value = 0;
    if (step == 1U)
        *operation = VN_SERIAL_PROBE_NATIVE;
    else if (step == 2U)
        *operation = VN_SERIAL_PROBE_EMULATION;
    else if (step == 3U)
        *operation = VN_SERIAL_PROBE_ARBITER_E1;
    else if (step == 4U)
        *operation = VN_SERIAL_PROBE_ARBITER_INT1;
    else if (step == 5U)
        *operation = VN_SERIAL_PROBE_ARBITER_EXT1;
    else if (step == 6U)
        *operation = VN_SERIAL_PROBE_INIT;
    else if (step >= 7U && step <= 37U)
    {
        write_index = step - 7U;
        *operation = VN_SERIAL_PROBE_WRITE;
        *value = vn_serial_probe_write_bytes[write_index];
    }
    else if (step == 38U)
    {
        *operation = VN_SERIAL_PROBE_STATUS_RX;
        *value = 1U;
    }
    else if (step == 39U)
    {
        *operation = VN_SERIAL_PROBE_STATUS_TX;
        *value = 0U;
    }
}

static void vn_serial_probe_ensure_initialized(void)
{
    if (!vn_serial_probe_initialized)
        vn_serial_probe_reset();
}

static void vn_serial_ring_init(VnSerialRing *ring,
                                unsigned char *storage,
                                unsigned int size)
{
    ring->data = storage;
    ring->size = size;
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
    ring->high_water = 0;
}

static int vn_serial_ring_push(VnSerialRing *ring, unsigned char value)
{
    if (ring->count >= ring->size)
        return 0;

    ring->data[ring->head] = value;
    ring->head++;
    if (ring->head >= ring->size)
        ring->head = 0;
    ring->count++;
    if (ring->count > ring->high_water)
        ring->high_water = ring->count;
    return 1;
}

static int vn_serial_ring_peek(const VnSerialRing *ring,
                               unsigned char *value)
{
    if (ring->count == 0)
        return 0;
    if (value != 0)
        *value = ring->data[ring->tail];
    return 1;
}

static int vn_serial_ring_pop(VnSerialRing *ring, unsigned char *value)
{
    if (!vn_serial_ring_peek(ring, value))
        return 0;

    ring->tail++;
    if (ring->tail >= ring->size)
        ring->tail = 0;
    ring->count--;
    return 1;
}

static void vn_serial_history_add(unsigned char *history,
                                  unsigned int *count,
                                  unsigned int *next,
                                  unsigned char value)
{
    history[*next] = value;
    *next = (*next + 1U) % VN_SERIAL_HISTORY_SIZE;
    if (*count < VN_SERIAL_HISTORY_SIZE)
        (*count)++;
}

static unsigned int vn_serial_history_copy(const unsigned char *history,
                                           unsigned int count,
                                           unsigned int next,
                                           unsigned char *data,
                                           unsigned int capacity)
{
    unsigned int copy_count;
    unsigned int first;
    unsigned int i;

    if (data == 0 || capacity == 0)
        return 0;

    copy_count = count;
    if (copy_count > capacity)
        copy_count = capacity;
    first = (next + VN_SERIAL_HISTORY_SIZE - copy_count) %
            VN_SERIAL_HISTORY_SIZE;
    for (i = 0; i < copy_count; i++)
        data[i] = history[(first + i) % VN_SERIAL_HISTORY_SIZE];
    return copy_count;
}

static void vn_serial_set_error(int error)
{
    vn_serial_current_stats.last_error = error;
    vn_serial_current_status = VN_SERIAL_STATUS_ERROR;
}

static int vn_serial_firmware_result_ok(const VnSerialFirmwareResult *result)
{
    if (result->arbiter_error != 0)
    {
        vn_serial_set_error((int)result->arbiter_error);
        return 0;
    }
    if (result->x != 0)
    {
        vn_serial_set_error((int)result->x);
        return 0;
    }
    return 1;
}

static int vn_serial_firmware_write_slot(unsigned int slot,
                                         unsigned char value)
{
    VnSerialFirmwareResult result;

    vn_serial_fw_write_slot(slot, value, &result);
    return vn_serial_firmware_result_ok(&result);
}

static int vn_serial_send_command_slot(unsigned int slot,
                                       const char *command)
{
    if (!vn_serial_firmware_write_slot(slot, VN_FW_COMMAND))
        return 0;
    while (*command != '\0')
    {
        if (!vn_serial_firmware_write_slot(slot, (unsigned char)*command))
            return 0;
        command++;
    }
    return 1;
}

static const char *vn_serial_baud_command(long baud)
{
    if (baud == 1200L)
        return "8B";
    if (baud == 2400L)
        return "10B";
    if (baud == 9600L)
        return "14B";
    return 0;
}

static int vn_serial_configure(long baud)
{
    return vn_serial_configure_slot(VN_SERIAL_SLOT_PRINTER, baud);
}

static int vn_serial_configure_slot(unsigned int slot, long baud)
{
    VnSerialFirmwareResult result;
    const char *baud_command;

    baud_command = vn_serial_baud_command(baud);
    if (baud_command == 0)
        return 0;

    vn_serial_fw_init_slot(slot, &result);
    if (!vn_serial_firmware_result_ok(&result))
        return 0;

    if (!vn_serial_send_command_slot(slot, baud_command) ||
        !vn_serial_send_command_slot(slot, "0D") ||
        !vn_serial_send_command_slot(slot, "0P") ||
        !vn_serial_send_command_slot(slot, "CD") ||
        !vn_serial_send_command_slot(slot, "XD") ||
        !vn_serial_send_command_slot(slot, "FD") ||
        !vn_serial_send_command_slot(slot, "LD") ||
        !vn_serial_send_command_slot(slot, "ED") ||
        !vn_serial_send_command_slot(slot, "MD") ||
        !vn_serial_send_command_slot(slot, "BE"))
        return 0;

    return 1;
}

static void vn_serial_update_queue_stats(void)
{
    vn_serial_current_stats.rx_queued = vn_serial_rx_ring.count;
    vn_serial_current_stats.tx_queued = vn_serial_tx_ring.count;
    vn_serial_current_stats.rx_high_water = vn_serial_rx_ring.high_water;
    vn_serial_current_stats.tx_high_water = vn_serial_tx_ring.high_water;
}

static void vn_serial_count_firmware_error(unsigned int error)
{
    if ((error & VN_FW_ERROR_PARITY) != 0)
        vn_serial_current_stats.parity_errors++;
    if ((error & VN_FW_ERROR_FRAMING) != 0)
        vn_serial_current_stats.framing_errors++;
    if ((error & VN_FW_ERROR_OVERRUN) != 0)
        vn_serial_current_stats.overrun_errors++;
    vn_serial_current_stats.last_error = (int)error;
}

static int vn_serial_poll_rx_once(void)
{
    VnSerialFirmwareResult result;
    unsigned char value;

    vn_serial_fw_status(1U, &result);
    if (result.arbiter_error != 0)
    {
        vn_serial_set_error((int)result.arbiter_error);
        return -1;
    }
    if (result.x != 0)
    {
        vn_serial_count_firmware_error(result.x);
        return 0;
    }
    if (result.carry == 0)
        return 0;

    vn_serial_fw_read(&result);
    if (result.arbiter_error != 0)
    {
        vn_serial_set_error((int)result.arbiter_error);
        return -1;
    }
    if (result.x != 0)
    {
        vn_serial_count_firmware_error(result.x);
        return 1;
    }

    value = (unsigned char)result.a;
    vn_serial_current_stats.rx_bytes++;
    vn_serial_history_add(vn_serial_rx_history,
                          &vn_serial_rx_history_count,
                          &vn_serial_rx_history_next,
                          value);
    if (!vn_serial_ring_push(&vn_serial_rx_ring, value))
        vn_serial_current_stats.rx_overflows++;
    return 1;
}

static int vn_serial_poll_tx_once(void)
{
    VnSerialFirmwareResult result;
    unsigned char payload;
    unsigned char output;

    if (!vn_serial_ring_peek(&vn_serial_tx_ring, &payload))
        return 0;

    vn_serial_fw_status(0U, &result);
    if (result.arbiter_error != 0)
    {
        vn_serial_set_error((int)result.arbiter_error);
        return -1;
    }
    if (result.x != 0)
    {
        vn_serial_set_error((int)result.x);
        return -1;
    }
    if (result.carry == 0)
        return 0;

    if (payload == VN_FW_COMMAND)
        output = vn_serial_escape_bytes[vn_serial_escape_index];
    else
        output = payload;

    vn_serial_fw_write(output, &result);
    if (!vn_serial_firmware_result_ok(&result))
        return -1;

    if (payload == VN_FW_COMMAND)
    {
        vn_serial_escape_index++;
        if (vn_serial_escape_index < sizeof(vn_serial_escape_bytes))
            return 1;
        vn_serial_escape_index = 0;
    }

    vn_serial_ring_pop(&vn_serial_tx_ring, &payload);
    vn_serial_current_stats.tx_bytes++;
    vn_serial_history_add(vn_serial_tx_history,
                          &vn_serial_tx_history_count,
                          &vn_serial_tx_history_next,
                          payload);
    return 1;
}

int vn_serial_open(int slot, long baud)
{
    vn_serial_close();
    memset(&vn_serial_current_stats, 0, sizeof(vn_serial_current_stats));
    memset(vn_serial_rx_history, 0, sizeof(vn_serial_rx_history));
    memset(vn_serial_tx_history, 0, sizeof(vn_serial_tx_history));
    vn_serial_ring_init(&vn_serial_rx_ring,
                        vn_serial_rx_storage,
                        VN_SERIAL_RX_BUFFER_SIZE);
    vn_serial_ring_init(&vn_serial_tx_ring,
                        vn_serial_tx_storage,
                        VN_SERIAL_TX_BUFFER_SIZE);
    vn_serial_rx_history_count = 0;
    vn_serial_rx_history_next = 0;
    vn_serial_tx_history_count = 0;
    vn_serial_tx_history_next = 0;
    vn_serial_escape_index = 0;
    if (slot != VN_SERIAL_SLOT_PRINTER)
    {
        vn_serial_set_error(-1);
        return 0;
    }
    if (vn_serial_baud_command(baud) == 0)
    {
        vn_serial_set_error(-2);
        return 0;
    }
    if (!vn_serial_configure(baud))
        return 0;

    vn_serial_current_status = VN_SERIAL_STATUS_OPEN;
    return 1;
}

void vn_serial_close(void)
{
#if VN_SERIAL_CLOSE_RESTORE_ENABLED
    VnSerialFirmwareResult result;

    if (vn_serial_current_status == VN_SERIAL_STATUS_OPEN)
        vn_serial_fw_init(&result);
#endif

    vn_serial_current_status = VN_SERIAL_STATUS_CLOSED;
    vn_serial_escape_index = 0;
    vn_serial_ring_init(&vn_serial_rx_ring,
                        vn_serial_rx_storage,
                        VN_SERIAL_RX_BUFFER_SIZE);
    vn_serial_ring_init(&vn_serial_tx_ring,
                        vn_serial_tx_storage,
                        VN_SERIAL_TX_BUFFER_SIZE);
    vn_serial_update_queue_stats();
}

void vn_serial_poll(void)
{
    unsigned int i;
    int result;

    if (vn_serial_current_status != VN_SERIAL_STATUS_OPEN)
        return;

    for (i = 0; i < VN_SERIAL_POLL_BUDGET; i++)
    {
        result = vn_serial_poll_rx_once();
        if (result <= 0)
            break;
    }

    if (vn_serial_current_status == VN_SERIAL_STATUS_OPEN)
    {
        for (i = 0; i < VN_SERIAL_POLL_BUDGET; i++)
        {
            result = vn_serial_poll_tx_once();
            if (result <= 0)
                break;
        }
    }
    vn_serial_update_queue_stats();
}

int vn_serial_read_byte(unsigned char *value)
{
    if (value == 0)
        return -1;
    if (vn_serial_current_status == VN_SERIAL_STATUS_ERROR)
        return -1;
    if (!vn_serial_ring_pop(&vn_serial_rx_ring, value))
        return 0;
    vn_serial_update_queue_stats();
    return 1;
}

unsigned int vn_serial_write(const unsigned char *data,
                             unsigned int length)
{
    unsigned int accepted;

    if (data == 0 || vn_serial_current_status != VN_SERIAL_STATUS_OPEN)
        return 0;

    accepted = 0;
    while (accepted < length)
    {
        if (!vn_serial_ring_push(&vn_serial_tx_ring, data[accepted]))
        {
            vn_serial_current_stats.tx_overflows++;
            break;
        }
        accepted++;
    }
    vn_serial_update_queue_stats();
    return accepted;
}

static unsigned int vn_serial_diag_write_raw_slot(unsigned int slot,
                                                  const unsigned char *data,
                                                  unsigned int length)
{
    VnSerialFirmwareResult result;
    unsigned int written;

    if (data == 0)
        return 0;

    written = 0;
    while (written < length)
    {
        vn_serial_fw_status_slot(slot, 0U, &result);
        if (result.arbiter_error != 0)
        {
            vn_serial_set_error((int)result.arbiter_error);
            break;
        }
        if (result.x != 0)
        {
            vn_serial_set_error((int)result.x);
            break;
        }
        if (result.carry == 0)
            break;

        vn_serial_fw_write_slot(slot, data[written], &result);
        if (!vn_serial_firmware_result_ok(&result))
            break;

        vn_serial_current_stats.tx_bytes++;
        vn_serial_history_add(vn_serial_tx_history,
                              &vn_serial_tx_history_count,
                              &vn_serial_tx_history_next,
                              data[written]);
        written++;
    }
    vn_serial_update_queue_stats();
    return written;
}

unsigned int vn_serial_diag_write_raw(const unsigned char *data,
                                      unsigned int length)
{
    if (vn_serial_current_status != VN_SERIAL_STATUS_OPEN)
        return 0;

    return vn_serial_diag_write_raw_slot(VN_SERIAL_SLOT_PRINTER,
                                         data, length);
}

unsigned int vn_serial_diag_write_raw_printer_unconfigured(
    const unsigned char *data,
    unsigned int length)
{
    VnSerialFirmwareResult result;

    if (data == 0)
        return 0;

    vn_serial_fw_init_slot(VN_SERIAL_SLOT_PRINTER, &result);
    if (!vn_serial_firmware_result_ok(&result))
        return 0;

    return vn_serial_diag_write_raw_slot(VN_SERIAL_SLOT_PRINTER,
                                         data, length);
}

unsigned int vn_serial_diag_write_raw_modem(long baud,
                                            const unsigned char *data,
                                            unsigned int length)
{
    if (data == 0)
        return 0;
    if (vn_serial_baud_command(baud) == 0)
    {
        vn_serial_set_error(-2);
        return 0;
    }
    if (!vn_serial_configure_slot(VN_SERIAL_SLOT_MODEM, baud))
        return 0;

    return vn_serial_diag_write_raw_slot(VN_SERIAL_SLOT_MODEM, data, length);
}

static int vn_serial_diag_status_tx_slot(unsigned int slot)
{
    VnSerialFirmwareResult result;

    vn_serial_fw_status_slot(slot, 0U, &result);
    if (result.arbiter_error != 0)
    {
        vn_serial_set_error((int)result.arbiter_error);
        return -1;
    }
    if (result.x != 0)
    {
        vn_serial_set_error((int)result.x);
        return -1;
    }
    if (result.carry == 0)
        return 0;
    return 1;
}

static int vn_serial_diag_write_byte_slot(unsigned int slot,
                                          unsigned char value)
{
    VnSerialFirmwareResult result;

    vn_serial_fw_write_slot(slot, value, &result);
    if (!vn_serial_firmware_result_ok(&result))
        return 0;

    vn_serial_current_stats.tx_bytes++;
    vn_serial_history_add(vn_serial_tx_history,
                          &vn_serial_tx_history_count,
                          &vn_serial_tx_history_next,
                          value);
    vn_serial_update_queue_stats();
    return 1;
}

int vn_serial_diag_modem_init(void)
{
    VnSerialFirmwareResult result;

    vn_serial_fw_init_slot(VN_SERIAL_SLOT_MODEM, &result);
    return vn_serial_firmware_result_ok(&result);
}

int vn_serial_diag_modem_status_tx(void)
{
    return vn_serial_diag_status_tx_slot(VN_SERIAL_SLOT_MODEM);
}

int vn_serial_diag_modem_write_byte(unsigned char value)
{
    return vn_serial_diag_write_byte_slot(VN_SERIAL_SLOT_MODEM, value);
}

int vn_serial_diag_printer_status_tx(void)
{
    return vn_serial_diag_status_tx_slot(VN_SERIAL_SLOT_PRINTER);
}

int vn_serial_diag_printer_write_byte(unsigned char value)
{
    return vn_serial_diag_write_byte_slot(VN_SERIAL_SLOT_PRINTER, value);
}

static unsigned int vn_serial_mode_bit23(unsigned long value)
{
    return (value & VN_SERIAL_MODE_BIT23) != 0UL ? 1U : 0U;
}

static void vn_serial_mode_set_stage(VnSerialModeStage stage)
{
    vn_serial_mode_current.stage = stage;
}

static int vn_serial_mode_call_failed(const VnSerialModeCallResult *result)
{
    return result->firmware.arbiter_error != 0 ||
           result->firmware.carry != 0 ||
           result->result_code != 0;
}

static void vn_serial_mode_record_call(const VnSerialModeCallResult *result)
{
    vn_serial_mode_current.dispatch_offset = result->dispatch_offset;
    vn_serial_mode_current.dispatch_address = result->dispatch_address;
    vn_serial_mode_current.checkpoint = result->checkpoint;
    vn_serial_mode_current.pointer_a = result->pointer_a;
    vn_serial_mode_current.pointer_x = result->pointer_x;
    vn_serial_mode_current.pointer_y = result->pointer_y;
    vn_serial_mode_current.command_word0 = result->command_word0;
    vn_serial_mode_current.command_word1 = result->command_word1;
    vn_serial_mode_current.command_word2 = result->command_word2;
    vn_serial_mode_current.command_word3 = result->command_word3;
    vn_serial_mode_current.processor_status = result->processor_status;
    vn_serial_mode_current.data_bank = result->data_bank;
    vn_serial_mode_current.direct_page = result->direct_page;
    vn_serial_mode_current.saved_stack = result->saved_stack;
    vn_serial_mode_current.returned_mode_bits = result->mode_bits;
    vn_serial_mode_current.result_code = result->result_code;
    vn_serial_mode_current.carry = result->firmware.carry;
    vn_serial_mode_current.arbiter_error = result->firmware.arbiter_error;
    vn_serial_mode_current.bit23 =
        vn_serial_mode_bit23(result->mode_bits);
}

static int vn_serial_mode_fail(VnSerialModeStage stage)
{
    vn_serial_mode_current.failure_stage = stage;
    vn_serial_mode_current.stage = VN_SERIAL_MODE_STAGE_FAILED;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_FAILED;
    return -1;
}

static void vn_serial_mode_advance_step(void)
{
    if (vn_serial_mode_current.step < vn_serial_mode_current.total_steps)
        vn_serial_mode_current.step++;
}

static void vn_serial_mode_ensure_initialized(void)
{
    if (!vn_serial_mode_initialized)
        vn_serial_mode_reset();
}

void vn_serial_mode_reset(void)
{
    memset(&vn_serial_mode_current, 0, sizeof(vn_serial_mode_current));
    vn_serial_mode_current.step = 1U;
    vn_serial_mode_current.total_steps = VN_SERIAL_MODE_TOTAL_STEPS;
    vn_serial_mode_current.stage = VN_SERIAL_MODE_STAGE_LOCATE;
    vn_serial_mode_current.failure_stage = VN_SERIAL_MODE_STAGE_LOCATE;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_READY;
    vn_serial_mode_initialized = 1;
}

void vn_serial_mode8n1_reset(void)
{
    memset(&vn_serial_mode_current, 0, sizeof(vn_serial_mode_current));
    vn_serial_mode_current.step = 1U;
    vn_serial_mode_current.total_steps = VN_SERIAL_MODE_8N1_TOTAL_STEPS;
    vn_serial_mode_current.stage = VN_SERIAL_MODE_STAGE_LOCATE;
    vn_serial_mode_current.failure_stage = VN_SERIAL_MODE_STAGE_LOCATE;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_READY;
    vn_serial_mode_current.configured_setup = 1U;
    vn_serial_mode_initialized = 1;
}

static int vn_serial_mode_step_locate(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_fw_mode_locate(&result);
    vn_serial_mode_record_call(&result);
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    if (vn_serial_mode_current.configured_setup)
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_INIT);
    else
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_GET_ORIGINAL);
    return 1;
}

static int vn_serial_mode_step_init(void)
{
    VnSerialFirmwareResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_init_slot(VN_SERIAL_SLOT_PRINTER, &result);
    vn_serial_mode_current.carry = result.carry;
    vn_serial_mode_current.arbiter_error = result.arbiter_error;
    if (!vn_serial_firmware_result_ok(&result))
    {
        vn_serial_mode_current.result_code =
            (unsigned int)vn_serial_current_stats.last_error;
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_INIT);
    }

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_GET_ORIGINAL);
    return 1;
}

static int vn_serial_mode_step_setup_status(void)
{
    int result;

    if (vn_serial_mode_current.setup_index >=
        sizeof(vn_serial_probe_write_bytes))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_SETUP_STATUS);

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_mode_current.setup_value =
        (unsigned int)vn_serial_probe_write_bytes[
            vn_serial_mode_current.setup_index];
    result = vn_serial_diag_printer_status_tx();
    if (result > 0)
    {
        vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
        vn_serial_mode_advance_step();
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_SETUP_WRITE);
        return 1;
    }
    if (result == 0)
    {
        vn_serial_mode_current.outcome = VN_SERIAL_MODE_WAIT;
        return 0;
    }
    vn_serial_mode_current.result_code =
        (unsigned int)vn_serial_current_stats.last_error;
    return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_SETUP_STATUS);
}

static int vn_serial_mode_step_setup_write(void)
{
    unsigned char value;
    int result;

    if (vn_serial_mode_current.setup_index >=
        sizeof(vn_serial_probe_write_bytes))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_SETUP_WRITE);

    value = vn_serial_probe_write_bytes[vn_serial_mode_current.setup_index];
    vn_serial_mode_current.setup_value = (unsigned int)value;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    result = vn_serial_diag_printer_write_byte(value);
    if (!result)
    {
        vn_serial_mode_current.result_code =
            (unsigned int)vn_serial_current_stats.last_error;
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_SETUP_WRITE);
    }

    vn_serial_mode_current.setup_accepted++;
    vn_serial_mode_current.setup_index++;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    if (vn_serial_mode_current.setup_index >=
        sizeof(vn_serial_probe_write_bytes))
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_PREPARE);
    else
    {
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_SETUP_STATUS);
        vn_serial_mode_current.setup_value =
            (unsigned int)vn_serial_probe_write_bytes[
                vn_serial_mode_current.setup_index];
    }
    return 1;
}

static int vn_serial_mode_step_get_original(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_get_mode_bits(&result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_GET_ORIGINAL);

    vn_serial_mode_current.original_mode_bits = result.mode_bits;
    vn_serial_mode_current.original_valid = 1U;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    if (vn_serial_mode_current.configured_setup)
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_CLEAR_BIT23);
    else
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_PREPARE);
    return 1;
}

static int vn_serial_mode_step_clear_bit23(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_clear_mode_bit23_atomic(&result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_CLEAR_BIT23);

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS);
    return 1;
}

static int vn_serial_mode_step_verify_commands(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_get_mode_bits(&result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS);
    if (vn_serial_mode_bit23(result.mode_bits))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS);

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_current.setup_index = 0U;
    vn_serial_mode_current.setup_value =
        (unsigned int)vn_serial_probe_write_bytes[0];
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_SETUP_STATUS);
    return 1;
}

static int vn_serial_mode_step_prepare(void)
{
    if (!vn_serial_mode_current.original_valid)
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_PREPARE);

    vn_serial_mode_current.modified_mode_bits =
        vn_serial_mode_current.original_mode_bits | VN_SERIAL_MODE_BIT23;
    vn_serial_mode_current.returned_mode_bits =
        vn_serial_mode_current.modified_mode_bits;
    vn_serial_mode_current.bit23 =
        vn_serial_mode_bit23(vn_serial_mode_current.modified_mode_bits);
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_SET_BIT23);
    return 1;
}

static int vn_serial_mode_step_set_bit23(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_set_mode_bit23_atomic(&result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_SET_BIT23);

    vn_serial_mode_current.modified_mode_bits = result.mode_bits;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_VERIFY_BIT23);
    return 1;
}

static int vn_serial_mode_step_verify_bit23(void)
{
    VnSerialModeCallResult result;
    unsigned long expected_other_bits;
    unsigned long returned_other_bits;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_get_mode_bits(&result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_BIT23);

    expected_other_bits =
        vn_serial_mode_current.original_mode_bits & ~VN_SERIAL_MODE_BIT23;
    returned_other_bits = result.mode_bits & ~VN_SERIAL_MODE_BIT23;
    vn_serial_mode_current.unexpected_changes =
        expected_other_bits == returned_other_bits ? 0U : 1U;
    if (!vn_serial_mode_bit23(result.mode_bits))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_BIT23);

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS);
    vn_serial_mode_current.payload_index = 0U;
    vn_serial_mode_current.payload_value =
        (unsigned int)vn_serial_mode_payload[0];
    return 1;
}

static int vn_serial_mode_step_payload_status(void)
{
    int result;

    if (vn_serial_mode_current.payload_index >=
        sizeof(vn_serial_mode_payload))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS);

    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_mode_current.payload_value =
        (unsigned int)vn_serial_mode_payload[
            vn_serial_mode_current.payload_index];
    result = vn_serial_diag_printer_status_tx();
    if (result > 0)
    {
        vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
        vn_serial_mode_advance_step();
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE);
        return 1;
    }
    if (result == 0)
    {
        vn_serial_mode_current.outcome = VN_SERIAL_MODE_WAIT;
        return 0;
    }
    vn_serial_mode_current.result_code =
        (unsigned int)vn_serial_current_stats.last_error;
    return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS);
}

static int vn_serial_mode_step_payload_write(void)
{
    unsigned char value;
    int result;

    if (vn_serial_mode_current.payload_index >=
        sizeof(vn_serial_mode_payload))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE);

    value = vn_serial_mode_payload[vn_serial_mode_current.payload_index];
    vn_serial_mode_current.payload_value = (unsigned int)value;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    result = vn_serial_diag_printer_write_byte(value);
    if (!result)
    {
        vn_serial_mode_current.result_code =
            (unsigned int)vn_serial_current_stats.last_error;
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE);
    }

    vn_serial_mode_current.payload_accepted++;
    vn_serial_mode_current.payload_index++;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    if (vn_serial_mode_current.payload_index >=
        sizeof(vn_serial_mode_payload))
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_RESTORE);
    else
    {
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS);
        vn_serial_mode_current.payload_value =
            (unsigned int)vn_serial_mode_payload[
                vn_serial_mode_current.payload_index];
    }
    return 1;
}

static int vn_serial_mode_step_restore(void)
{
    VnSerialModeCallResult result;

    if (!vn_serial_mode_current.original_valid)
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_RESTORE);

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_set_mode_bits(vn_serial_mode_current.original_mode_bits,
                               &result);
    vn_serial_mode_record_call(&result);
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_RESTORE);

    vn_serial_mode_current.restore_done = 1U;
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_PASSED;
    vn_serial_mode_advance_step();
    vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_VERIFY_RESTORE);
    return 1;
}

static int vn_serial_mode_step_verify_restore(void)
{
    VnSerialModeCallResult result;

    memset(&result, 0, sizeof(result));
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_IN_FLIGHT;
    vn_serial_fw_get_mode_bits(&result);
    vn_serial_mode_record_call(&result);
    vn_serial_mode_current.restored_mode_bits = result.mode_bits;
    if (vn_serial_mode_call_failed(&result))
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_RESTORE);
    if (result.mode_bits != vn_serial_mode_current.original_mode_bits)
        return vn_serial_mode_fail(VN_SERIAL_MODE_STAGE_VERIFY_RESTORE);

    vn_serial_mode_current.bit23 =
        vn_serial_mode_bit23(result.mode_bits);
    vn_serial_mode_current.outcome = VN_SERIAL_MODE_COMPLETE;
    vn_serial_mode_current.stage = VN_SERIAL_MODE_STAGE_COMPLETE;
    vn_serial_mode_current.step = vn_serial_mode_current.total_steps;
    return 1;
}

int vn_serial_mode_next(void)
{
    vn_serial_mode_ensure_initialized();

    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_FAILED ||
        vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_COMPLETE)
        return 0;

    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_LOCATE)
        return vn_serial_mode_step_locate();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_INIT)
        return vn_serial_mode_step_init();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_CLEAR_BIT23)
        return vn_serial_mode_step_clear_bit23();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS)
        return vn_serial_mode_step_verify_commands();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_SETUP_STATUS)
        return vn_serial_mode_step_setup_status();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_SETUP_WRITE)
        return vn_serial_mode_step_setup_write();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_GET_ORIGINAL)
        return vn_serial_mode_step_get_original();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_PREPARE)
        return vn_serial_mode_step_prepare();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_SET_BIT23)
        return vn_serial_mode_step_set_bit23();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_VERIFY_BIT23)
        return vn_serial_mode_step_verify_bit23();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS)
        return vn_serial_mode_step_payload_status();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE)
        return vn_serial_mode_step_payload_write();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_RESTORE)
        return vn_serial_mode_step_restore();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_VERIFY_RESTORE)
        return vn_serial_mode_step_verify_restore();
    return 0;
}

int vn_serial_mode_prepare_restore(void)
{
    vn_serial_mode_ensure_initialized();
    if (!vn_serial_mode_current.original_valid)
        return 0;
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_COMPLETE)
        return 0;
    if (vn_serial_mode_current.stage != VN_SERIAL_MODE_STAGE_RESTORE &&
        vn_serial_mode_current.stage != VN_SERIAL_MODE_STAGE_VERIFY_RESTORE)
        vn_serial_mode_set_stage(VN_SERIAL_MODE_STAGE_RESTORE);
    return 1;
}

int vn_serial_mode_restore_next(void)
{
    vn_serial_mode_ensure_initialized();
    if (!vn_serial_mode_current.original_valid)
        return 0;
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_COMPLETE)
        return 0;
    vn_serial_mode_prepare_restore();
    if (vn_serial_mode_current.stage == VN_SERIAL_MODE_STAGE_RESTORE)
        return vn_serial_mode_step_restore();
    return vn_serial_mode_step_verify_restore();
}

const VnSerialModeStatus *vn_serial_mode_status(void)
{
    vn_serial_mode_ensure_initialized();
    return &vn_serial_mode_current;
}

const char *vn_serial_mode_stage_text(VnSerialModeStage stage)
{
    if (stage == VN_SERIAL_MODE_STAGE_LOCATE)
        return "LOCATE EXT";
    if (stage == VN_SERIAL_MODE_STAGE_INIT)
        return "PINIT";
    if (stage == VN_SERIAL_MODE_STAGE_CLEAR_BIT23)
        return "CLEAR BIT23";
    if (stage == VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS)
        return "VERIFY COMMANDS";
    if (stage == VN_SERIAL_MODE_STAGE_SETUP_STATUS)
        return "SETUP STATUS";
    if (stage == VN_SERIAL_MODE_STAGE_SETUP_WRITE)
        return "SETUP WRITE";
    if (stage == VN_SERIAL_MODE_STAGE_GET_ORIGINAL)
        return "GET ORIGINAL";
    if (stage == VN_SERIAL_MODE_STAGE_PREPARE)
        return "PREPARE BIT23";
    if (stage == VN_SERIAL_MODE_STAGE_SET_BIT23)
        return "SET BIT23";
    if (stage == VN_SERIAL_MODE_STAGE_VERIFY_BIT23)
        return "VERIFY BIT23";
    if (stage == VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS)
        return "PAYLOAD STATUS";
    if (stage == VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE)
        return "PAYLOAD WRITE";
    if (stage == VN_SERIAL_MODE_STAGE_RESTORE)
        return "RESTORE ORIGINAL";
    if (stage == VN_SERIAL_MODE_STAGE_VERIFY_RESTORE)
        return "VERIFY RESTORE";
    if (stage == VN_SERIAL_MODE_STAGE_COMPLETE)
        return "COMPLETE";
    if (stage == VN_SERIAL_MODE_STAGE_FAILED)
        return "FAILED";
    return "UNKNOWN";
}

VnSerialStatus vn_serial_status(void)
{
    return vn_serial_current_status;
}

const VnSerialStats *vn_serial_stats(void)
{
    return &vn_serial_current_stats;
}

const char *vn_serial_status_text(void)
{
    if (vn_serial_current_status == VN_SERIAL_STATUS_OPEN)
        return "OPEN";
    if (vn_serial_current_status == VN_SERIAL_STATUS_CLOSED)
        return "CLOSED";

    sprintf(vn_serial_status_buffer, "ERROR %02X",
            (unsigned int)vn_serial_current_stats.last_error & 0xFFU);
    return vn_serial_status_buffer;
}

unsigned int vn_serial_recent_rx(unsigned char *data,
                                 unsigned int capacity)
{
    return vn_serial_history_copy(vn_serial_rx_history,
                                  vn_serial_rx_history_count,
                                  vn_serial_rx_history_next,
                                  data, capacity);
}

unsigned int vn_serial_recent_tx(unsigned char *data,
                                 unsigned int capacity)
{
    return vn_serial_history_copy(vn_serial_tx_history,
                                  vn_serial_tx_history_count,
                                  vn_serial_tx_history_next,
                                  data, capacity);
}

int vn_serial_ring_self_test(void)
{
    VnSerialRing ring;
    unsigned char value;
    unsigned int i;

    vn_serial_ring_init(&ring, vn_serial_test_storage,
                        sizeof(vn_serial_test_storage));
    for (i = 0; i < sizeof(vn_serial_test_storage); i++)
    {
        if (!vn_serial_ring_push(&ring, (unsigned char)i))
            return 0;
    }
    if (vn_serial_ring_push(&ring, 0xFFU))
        return 0;
    for (i = 0; i < 9U; i++)
    {
        if (!vn_serial_ring_pop(&ring, &value) || value != (unsigned char)i)
            return 0;
    }
    for (i = 0; i < 9U; i++)
    {
        if (!vn_serial_ring_push(&ring, (unsigned char)(0x80U + i)))
            return 0;
    }
    for (i = 9U; i < 17U; i++)
    {
        if (!vn_serial_ring_pop(&ring, &value) || value != (unsigned char)i)
            return 0;
    }
    for (i = 0; i < 9U; i++)
    {
        if (!vn_serial_ring_pop(&ring, &value) ||
            value != (unsigned char)(0x80U + i))
            return 0;
    }
    return ring.count == 0 && ring.high_water == 17U;
}

void vn_serial_probe_reset(void)
{
    memset(&vn_serial_probe_current, 0, sizeof(vn_serial_probe_current));
    vn_serial_probe_current.next_step = 1U;
    vn_serial_probe_current.total_steps = VN_SERIAL_PROBE_TOTAL_STEPS;
    vn_serial_probe_current.outcome = VN_SERIAL_PROBE_READY;
    vn_serial_probe_describe(vn_serial_probe_current.next_step,
                             &vn_serial_probe_current.next_operation,
                             &vn_serial_probe_current.next_value);
    vn_serial_probe_initialized = 1;
}

int vn_serial_probe_next(void)
{
    VnSerialFirmwareResult result;
    VnSerialProbeOperation operation;
    unsigned int value;
    int failed;

    vn_serial_probe_ensure_initialized();
    if (vn_serial_probe_current.next_step > VN_SERIAL_PROBE_TOTAL_STEPS)
    {
        vn_serial_probe_current.outcome = VN_SERIAL_PROBE_COMPLETE;
        return 0;
    }

    operation = vn_serial_probe_current.next_operation;
    value = vn_serial_probe_current.next_value;
    vn_serial_probe_current.last_step = vn_serial_probe_current.next_step;
    vn_serial_probe_current.last_operation = operation;
    vn_serial_probe_current.last_value = value;
    vn_serial_probe_current.outcome = VN_SERIAL_PROBE_IN_FLIGHT;
    memset(&result, 0, sizeof(result));

    if (operation == VN_SERIAL_PROBE_NATIVE)
        vn_serial_fw_probe_native(&result);
    else if (operation == VN_SERIAL_PROBE_EMULATION)
        vn_serial_fw_probe_emulation(&result);
    else if (operation == VN_SERIAL_PROBE_ARBITER)
        vn_serial_fw_probe_arbiter_query(&result);
    else if (operation == VN_SERIAL_PROBE_ARBITER_E1)
        vn_serial_fw_probe_arbiter_value(0x8000U, &result);
    else if (operation == VN_SERIAL_PROBE_ARBITER_INT1)
        vn_serial_fw_probe_arbiter_request(0x0001U, &result);
    else if (operation == VN_SERIAL_PROBE_ARBITER_EXT1)
        vn_serial_fw_probe_arbiter_request(0x0009U, &result);
    else if (operation == VN_SERIAL_PROBE_INIT)
        vn_serial_fw_probe_init_fwentry(&result);
    else if (operation == VN_SERIAL_PROBE_WRITE)
        vn_serial_fw_write((unsigned char)value, &result);
    else if (operation == VN_SERIAL_PROBE_STATUS_RX)
        vn_serial_fw_status(1U, &result);
    else if (operation == VN_SERIAL_PROBE_STATUS_TX)
        vn_serial_fw_status(0U, &result);

    vn_serial_probe_current.a = result.a;
    vn_serial_probe_current.x = result.x;
    vn_serial_probe_current.y = result.y;
    vn_serial_probe_current.carry = result.carry;
    vn_serial_probe_current.arbiter_error = result.arbiter_error;

    failed = result.arbiter_error != 0 || result.carry != 0;

    if (operation == VN_SERIAL_PROBE_ARBITER_E1)
    {
        vn_serial_probe_current.outcome = VN_SERIAL_PROBE_INFO;
        vn_serial_probe_current.next_step++;
        vn_serial_probe_describe(vn_serial_probe_current.next_step,
                                 &vn_serial_probe_current.next_operation,
                                 &vn_serial_probe_current.next_value);
        return 1;
    }

    if (operation == VN_SERIAL_PROBE_ARBITER_EXT1)
    {
        vn_serial_probe_current.outcome = VN_SERIAL_PROBE_INFO;
        vn_serial_probe_current.next_step++;
        vn_serial_probe_describe(vn_serial_probe_current.next_step,
                                 &vn_serial_probe_current.next_operation,
                                 &vn_serial_probe_current.next_value);
        return 1;
    }

    if (operation == VN_SERIAL_PROBE_STATUS_RX ||
        operation == VN_SERIAL_PROBE_STATUS_TX)
    {
        failed = result.arbiter_error != 0 || result.x != 0;
        if (failed)
        {
            vn_serial_probe_current.outcome = VN_SERIAL_PROBE_FAILED;
            return -1;
        }

        vn_serial_probe_current.outcome = VN_SERIAL_PROBE_PASSED;
        vn_serial_probe_current.next_step++;
        vn_serial_probe_describe(vn_serial_probe_current.next_step,
                                 &vn_serial_probe_current.next_operation,
                                 &vn_serial_probe_current.next_value);
        return 1;
    }

    if (failed)
    {
        vn_serial_probe_current.outcome = VN_SERIAL_PROBE_FAILED;
        return -1;
    }

    vn_serial_probe_current.outcome = VN_SERIAL_PROBE_PASSED;
    vn_serial_probe_current.next_step++;
    vn_serial_probe_describe(vn_serial_probe_current.next_step,
                             &vn_serial_probe_current.next_operation,
                             &vn_serial_probe_current.next_value);
    return 1;
}

const VnSerialProbeStatus *vn_serial_probe_status(void)
{
    vn_serial_probe_ensure_initialized();
    return &vn_serial_probe_current;
}

const char *vn_serial_probe_operation_text(VnSerialProbeOperation operation)
{
    if (operation == VN_SERIAL_PROBE_NATIVE)
        return "NATIVE RETURN";
    if (operation == VN_SERIAL_PROBE_EMULATION)
        return "EMU RETURN";
    if (operation == VN_SERIAL_PROBE_ARBITER)
        return "SLOT ARB $01FCBC";
    if (operation == VN_SERIAL_PROBE_ARBITER_E1)
        return "ARB QUERY $8000";
    if (operation == VN_SERIAL_PROBE_ARBITER_INT1)
        return "ARB INT1 $0001";
    if (operation == VN_SERIAL_PROBE_ARBITER_EXT1)
        return "ARB EXT1 $0009";
    if (operation == VN_SERIAL_PROBE_INIT)
        return "PINIT";
    if (operation == VN_SERIAL_PROBE_WRITE)
        return "PWRITE";
    if (operation == VN_SERIAL_PROBE_STATUS_RX)
        return "PSTATUS RX";
    if (operation == VN_SERIAL_PROBE_STATUS_TX)
        return "PSTATUS TX";
    return "COMPLETE";
}
