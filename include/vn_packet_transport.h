#ifndef VN_PACKET_TRANSPORT_H
#define VN_PACKET_TRANSPORT_H

#include "include/vn_protocol.h"

#define VN_PACKET_TRANSPORT_RX_BUFFER_SIZE (VN_MAX_PACKET_SIZE * 2)

void vn_packet_transport_reset(void);
int vn_packet_transport_send(VnU8 message_type,
                             const VnU8 *payload,
                             VnU16 payload_length,
                             VnU16 sequence,
                             VnU16 flags,
                             VnU16 *packet_length,
                             unsigned int *accepted);
int vn_packet_transport_poll(VnPacket *packet);
unsigned int vn_packet_transport_last_tx(VnU8 *data,
                                         unsigned int capacity);
unsigned int vn_packet_transport_last_rx(VnU8 *data,
                                         unsigned int capacity);
int vn_packet_transport_rx_buffered(void);

#endif
