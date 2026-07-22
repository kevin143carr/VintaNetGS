#include <string.h>

#include "include/vn_protocol.h"

void vn_write_u16_le(VnU8 *p, VnU16 value)
{
    p[0] = (VnU8)(value & 0xFF);
    p[1] = (VnU8)((value >> 8) & 0xFF);
}

VnU16 vn_read_u16_le(const VnU8 *p)
{
    return (VnU16)(p[0] | ((VnU16)p[1] << 8));
}

void vn_write_u32_le(VnU8 *p, VnU32 value)
{
    p[0] = (VnU8)(value & 0xFF);
    p[1] = (VnU8)((value >> 8) & 0xFF);
    p[2] = (VnU8)((value >> 16) & 0xFF);
    p[3] = (VnU8)((value >> 24) & 0xFF);
}

VnU32 vn_read_u32_le(const VnU8 *p)
{
    return (VnU32)p[0] |
           ((VnU32)p[1] << 8) |
           ((VnU32)p[2] << 16) |
           ((VnU32)p[3] << 24);
}

VnU16 vn_crc16(const VnU8 *data, VnU16 length)
{
    VnU16 crc;
    VnU16 i;
    int bit;

    crc = 0xFFFF;
    for (i = 0; i < length; i++)
    {
        crc = (VnU16)(crc ^ ((VnU16)data[i] << 8));
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000) != 0)
                crc = (VnU16)((crc << 1) ^ 0x1021);
            else
                crc = (VnU16)(crc << 1);
        }
    }

    return crc;
}

void vn_build_header(VnU8 *buffer,
                     VnU8 message_type,
                     VnU16 payload_length,
                     VnU16 sequence,
                     VnU16 flags,
                     VnU16 crc16)
{
    buffer[0] = VN_MAGIC_1;
    buffer[1] = VN_MAGIC_2;
    buffer[2] = VN_VERSION_1;
    buffer[3] = message_type;
    vn_write_u16_le(buffer + 4, payload_length);
    vn_write_u16_le(buffer + 6, sequence);
    vn_write_u16_le(buffer + 8, flags);
    vn_write_u16_le(buffer + 10, crc16);
}

int vn_parse_header(const VnU8 *buffer, VnHeader *header)
{
    if (buffer[0] != VN_MAGIC_1 || buffer[1] != VN_MAGIC_2)
        return VN_ERR_BAD_MAGIC;
    if (buffer[2] != VN_VERSION_1)
        return VN_ERR_BAD_VERSION;

    header->magic1 = buffer[0];
    header->magic2 = buffer[1];
    header->version = buffer[2];
    header->msg_type = buffer[3];
    header->payload_len = vn_read_u16_le(buffer + 4);
    header->seq = vn_read_u16_le(buffer + 6);
    header->flags = vn_read_u16_le(buffer + 8);
    header->crc16 = vn_read_u16_le(buffer + 10);
    if (header->payload_len > VN_MAX_PAYLOAD)
        return VN_ERR_PAYLOAD_TOO_BIG;

    return VN_ERR_NONE;
}

int vn_build_packet(VnU8 *buffer,
                    VnU16 buffer_size,
                    VnU8 message_type,
                    const VnU8 *payload,
                    VnU16 payload_length,
                    VnU16 sequence,
                    VnU16 flags,
                    VnU16 *packet_length)
{
    VnU16 crc;

    if (payload_length > VN_MAX_PAYLOAD)
        return VN_ERR_PAYLOAD_TOO_BIG;
    if (buffer_size < (VnU16)(VN_HEADER_SIZE + payload_length))
        return VN_ERR_PAYLOAD_TOO_BIG;

    vn_build_header(buffer, message_type, payload_length, sequence, flags, 0);
    if (payload_length > 0 && payload != 0)
        memcpy(buffer + VN_HEADER_SIZE, payload, payload_length);

    crc = vn_crc16(buffer, (VnU16)(VN_HEADER_SIZE + payload_length));
    vn_write_u16_le(buffer + 10, crc);
    if (packet_length != 0)
        *packet_length = (VnU16)(VN_HEADER_SIZE + payload_length);
    return VN_ERR_NONE;
}

static void vn_discard_bytes(VnU8 *buffer, int *length, int count)
{
    if (count <= 0)
        return;
    if (count >= *length)
    {
        *length = 0;
        return;
    }

    memmove(buffer, buffer + count, *length - count);
    *length -= count;
}

int vn_extract_packet(VnU8 *receive_buffer,
                      int *receive_length,
                      VnPacket *packet)
{
    int i;
    int start;
    int total_length;
    int parse_result;
    VnU16 expected_crc;
    VnU8 saved_crc_low;
    VnU8 saved_crc_high;

    start = -1;
    for (i = 0; i < *receive_length - 1; i++)
    {
        if (receive_buffer[i] == VN_MAGIC_1 &&
            receive_buffer[i + 1] == VN_MAGIC_2)
        {
            start = i;
            break;
        }
    }

    if (start < 0)
    {
        if (*receive_length > 0)
        {
            if (receive_buffer[*receive_length - 1] == VN_MAGIC_1)
                vn_discard_bytes(receive_buffer, receive_length,
                                 *receive_length - 1);
            else
                vn_discard_bytes(receive_buffer, receive_length,
                                 *receive_length);
        }
        return 0;
    }

    if (start > 0)
        vn_discard_bytes(receive_buffer, receive_length, start);
    if (*receive_length < VN_HEADER_SIZE)
        return 0;

    parse_result = vn_parse_header(receive_buffer, &packet->header);
    if (parse_result != VN_ERR_NONE)
    {
        vn_discard_bytes(receive_buffer, receive_length, 2);
        return -parse_result;
    }

    total_length = VN_HEADER_SIZE + packet->header.payload_len;
    if (*receive_length < total_length)
        return 0;

    saved_crc_low = receive_buffer[10];
    saved_crc_high = receive_buffer[11];
    receive_buffer[10] = 0;
    receive_buffer[11] = 0;
    expected_crc = vn_crc16(receive_buffer, (VnU16)total_length);
    receive_buffer[10] = saved_crc_low;
    receive_buffer[11] = saved_crc_high;

    if (expected_crc != packet->header.crc16)
    {
        vn_discard_bytes(receive_buffer, receive_length, 2);
        return -VN_ERR_BAD_CRC;
    }

    if (packet->header.payload_len > 0)
    {
        memcpy(packet->payload,
               receive_buffer + VN_HEADER_SIZE,
               packet->header.payload_len);
    }
    vn_discard_bytes(receive_buffer, receive_length, total_length);
    return 1;
}
