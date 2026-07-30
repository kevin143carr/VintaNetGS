#include <string.h>

#include "include/vn_packet_transport.h"
#include "include/vn_serial.h"

static VnU8 vn_packet_transport_tx[VN_MAX_PACKET_SIZE];
static VnU16 vn_packet_transport_tx_length;
static VnU8 vn_packet_transport_rx[VN_PACKET_TRANSPORT_RX_BUFFER_SIZE];
static int vn_packet_transport_rx_length;
static VnU8 vn_packet_transport_last_rx_data[VN_MAX_PACKET_SIZE];
static unsigned int vn_packet_transport_last_rx_length;

void vn_packet_transport_reset(void)
{
    vn_packet_transport_tx_length = 0;
    vn_packet_transport_rx_length = 0;
    vn_packet_transport_last_rx_length = 0;
}

int vn_packet_transport_send(VnU8 message_type,
                             const VnU8 *payload,
                             VnU16 payload_length,
                             VnU16 sequence,
                             VnU16 flags,
                             VnU16 *packet_length,
                             unsigned int *accepted)
{
    int build_result;
    unsigned int written;

    if (accepted != 0)
        *accepted = 0;
    if (packet_length != 0)
        *packet_length = 0;
    vn_packet_transport_tx_length = 0;

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
        return VN_ERR_TIMEOUT;

    build_result = vn_build_packet(vn_packet_transport_tx,
                                   VN_MAX_PACKET_SIZE,
                                   message_type,
                                   payload,
                                   payload_length,
                                   sequence,
                                   flags,
                                   &vn_packet_transport_tx_length);
    if (build_result != VN_ERR_NONE)
        return build_result;

    written = vn_serial_write(vn_packet_transport_tx,
                              vn_packet_transport_tx_length);
    if (accepted != 0)
        *accepted = written;
    if (packet_length != 0)
        *packet_length = vn_packet_transport_tx_length;
    if (written != vn_packet_transport_tx_length)
        return VN_ERR_TIMEOUT;
    return VN_ERR_NONE;
}

int vn_packet_transport_poll(VnPacket *packet)
{
    unsigned char value;
    int extract_result;

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
        return -VN_ERR_TIMEOUT;

    vn_serial_poll();
    while (vn_serial_read_byte(&value) > 0)
    {
        if (vn_packet_transport_rx_length >=
            VN_PACKET_TRANSPORT_RX_BUFFER_SIZE)
        {
            vn_packet_transport_rx_length = 0;
            return -VN_ERR_PAYLOAD_TOO_BIG;
        }
        vn_packet_transport_rx[vn_packet_transport_rx_length] = value;
        vn_packet_transport_rx_length++;
        if (vn_packet_transport_last_rx_length < VN_MAX_PACKET_SIZE)
        {
            vn_packet_transport_last_rx_data[
                vn_packet_transport_last_rx_length] = value;
            vn_packet_transport_last_rx_length++;
        }
    }

    extract_result = vn_extract_packet(vn_packet_transport_rx,
                                       &vn_packet_transport_rx_length,
                                       packet);
    return extract_result;
}

unsigned int vn_packet_transport_last_tx(VnU8 *data,
                                         unsigned int capacity)
{
    unsigned int count;

    count = vn_packet_transport_tx_length;
    if (count > capacity)
        count = capacity;
    if (count > 0 && data != 0)
        memcpy(data, vn_packet_transport_tx, count);
    return count;
}

unsigned int vn_packet_transport_last_rx(VnU8 *data,
                                         unsigned int capacity)
{
    unsigned int count;

    count = vn_packet_transport_last_rx_length;
    if (count > capacity)
        count = capacity;
    if (count > 0 && data != 0)
        memcpy(data, vn_packet_transport_last_rx_data, count);
    return count;
}

int vn_packet_transport_rx_buffered(void)
{
    return vn_packet_transport_rx_length;
}
