#include <stdio.h>
#include <string.h>

#include "include/vn_serial.h"
#include "include/vn_serial_fw.h"

#define VN_SERIAL_SLOT_PRINTER 1
#define VN_SERIAL_POLL_BUDGET 32U

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

static const unsigned char vn_serial_escape_bytes[] = {
    VN_FW_COMMAND,
    VN_FW_ALT_COMMAND,
    VN_FW_COMMAND,
    VN_FW_ALT_COMMAND,
    VN_FW_COMMAND
};

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

static int vn_serial_firmware_write(unsigned char value)
{
    VnSerialFirmwareResult result;

    vn_serial_fw_write(value, &result);
    return vn_serial_firmware_result_ok(&result);
}

static int vn_serial_send_command(const char *command)
{
    if (!vn_serial_firmware_write(VN_FW_COMMAND))
        return 0;
    while (*command != '\0')
    {
        if (!vn_serial_firmware_write((unsigned char)*command))
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
    VnSerialFirmwareResult result;
    const char *baud_command;

    baud_command = vn_serial_baud_command(baud);
    if (baud_command == 0)
        return 0;

    vn_serial_fw_init(&result);
    if (!vn_serial_firmware_result_ok(&result))
        return 0;

    if (!vn_serial_send_command(baud_command) ||
        !vn_serial_send_command("0D") ||
        !vn_serial_send_command("0P") ||
        !vn_serial_send_command("CD") ||
        !vn_serial_send_command("XD") ||
        !vn_serial_send_command("FD") ||
        !vn_serial_send_command("LD") ||
        !vn_serial_send_command("ED") ||
        !vn_serial_send_command("MD") ||
        !vn_serial_send_command("BE"))
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
    VnSerialFirmwareResult result;

    if (vn_serial_current_status == VN_SERIAL_STATUS_OPEN)
        vn_serial_fw_init(&result);

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
