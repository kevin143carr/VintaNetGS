#ifndef VN_PROTOCOL_H
#define VN_PROTOCOL_H

#define VN_MAGIC_1       'V'
#define VN_MAGIC_2       'N'
#define VN_VERSION_1     1
#define VN_HEADER_SIZE   12
#define VN_MAX_PAYLOAD   512
#define VN_MAX_PACKET_SIZE (VN_HEADER_SIZE + VN_MAX_PAYLOAD)

typedef unsigned char  VnU8;
typedef unsigned short VnU16;
typedef unsigned long  VnU32;

typedef struct VnHeader {
    VnU8 magic1;
    VnU8 magic2;
    VnU8 version;
    VnU8 msg_type;
    VnU16 payload_len;
    VnU16 seq;
    VnU16 flags;
    VnU16 crc16;
} VnHeader;

typedef struct VnPacket {
    VnHeader header;
    VnU8 payload[VN_MAX_PAYLOAD];
} VnPacket;

#define VN_MSG_DISCOVERY_ANNOUNCE 0x02
#define VN_MSG_INFO_REQ           0x03
#define VN_MSG_INFO_RESP          0x04
#define VN_MSG_CAPS_REQ           0x05
#define VN_MSG_CAPS_RESP          0x06
#define VN_MSG_LAUNCH_REQ         0x10
#define VN_MSG_LAUNCH_RESP        0x11
#define VN_MSG_FILE_SEND_REQ      0x20
#define VN_MSG_FILE_RECV_REQ      0x21
#define VN_MSG_FILE_RESULT        0x22
#define VN_MSG_ACK                0x70
#define VN_MSG_NAK                0x71
#define VN_MSG_ERROR              0x72

#define VN_FLAG_NONE              0x0000
#define VN_FLAG_ACK_REQUIRED      0x0001
#define VN_FLAG_IS_RESPONSE       0x0002
#define VN_FLAG_IS_ERROR          0x0004
#define VN_FLAG_MORE_FRAGMENTS    0x0008
#define VN_FLAG_LAST_FRAGMENT     0x0010

#define VN_TLV_NODE_ID            0x01
#define VN_TLV_NODE_NAME          0x02
#define VN_TLV_ROLE               0x03
#define VN_TLV_CAP_FLAGS          0x04
#define VN_TLV_PROGRAM            0x05
#define VN_TLV_DOS_VERSION        0x06
#define VN_TLV_FREE_MEM           0x07
#define VN_TLV_SERIAL_SPEED       0x08
#define VN_TLV_ERROR_CODE         0x09
#define VN_TLV_ERROR_TEXT         0x0A
#define VN_TLV_FILE_PROTOCOL      0x0B
#define VN_TLV_COMMAND_TEXT       0x0C
#define VN_TLV_TARGET_ID          0x0D
#define VN_TLV_TARGET_NAME        0x0E
#define VN_TLV_INFO_REVISION      0x0F
#define VN_TLV_TTL                0x10
#define VN_TLV_REQUEST_ID         0x11
#define VN_TLV_HOP_COUNT          0x12
#define VN_TLV_KNOWN_NODE         0x13
#define VN_TLV_DISCOVERY_SESSION_ID 0x14
#define VN_TLV_DISCOVERY_SEQ      0x15

#define VN_CAP_ZMODEM_SEND        0x00000001L
#define VN_CAP_ZMODEM_RECV        0x00000002L
#define VN_CAP_XMODEM_SEND        0x00000004L
#define VN_CAP_XMODEM_RECV        0x00000008L
#define VN_CAP_LAUNCH_PROGRAM     0x00000010L
#define VN_CAP_REMOTE_REBOOT      0x00000020L
#define VN_CAP_FILE_LIST          0x00000040L
#define VN_CAP_TIME_SYNC          0x00000080L

#define VN_ERR_NONE               0
#define VN_ERR_BAD_MAGIC          1
#define VN_ERR_BAD_VERSION        2
#define VN_ERR_BAD_CRC            3
#define VN_ERR_PAYLOAD_TOO_BIG    4
#define VN_ERR_TIMEOUT            5
#define VN_ERR_MALFORMED_TLV      6
#define VN_ERR_UNKNOWN_MSG        7

void vn_write_u16_le(VnU8 *p, VnU16 value);
VnU16 vn_read_u16_le(const VnU8 *p);
void vn_write_u32_le(VnU8 *p, VnU32 value);
VnU32 vn_read_u32_le(const VnU8 *p);
VnU16 vn_crc16(const VnU8 *data, VnU16 length);

void vn_build_header(VnU8 *buffer,
                     VnU8 message_type,
                     VnU16 payload_length,
                     VnU16 sequence,
                     VnU16 flags,
                     VnU16 crc16);
int vn_parse_header(const VnU8 *buffer, VnHeader *header);
int vn_build_packet(VnU8 *buffer,
                    VnU16 buffer_size,
                    VnU8 message_type,
                    const VnU8 *payload,
                    VnU16 payload_length,
                    VnU16 sequence,
                    VnU16 flags,
                    VnU16 *packet_length);
int vn_extract_packet(VnU8 *receive_buffer,
                      int *receive_length,
                      VnPacket *packet);

#endif
