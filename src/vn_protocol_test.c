#include <string.h>

#include "include/vn_protocol.h"
#include "include/vn_tlv.h"
#include "include/vn_protocol_test.h"

static VnU8 test_payload[VN_MAX_PAYLOAD];
static VnU8 test_packet_buffer[VN_MAX_PACKET_SIZE];
static VnU8 test_receive_buffer[VN_MAX_PACKET_SIZE * 2];
static VnPacket test_packet;
static VnHeader test_header;
static VnTlv test_tlv;
static char test_string[16];

static const VnU8 test_vector[19] = {
    0x56, 0x4E, 0x01, 0x02, 0x07, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x7B, 0x47, 0x02, 0x04, 0x00, 0x49,
    0x49, 0x47, 0x53
};

static void test_clear_result(VnProtocolTestResult *result)
{
    result->total = 0;
    result->passed = 0;
    result->failed = 0;
    result->first_failed[0] = '\0';
}

static void test_copy_name(char *dest, const char *src)
{
    int i;

    for (i = 0; i < VN_PROTOCOL_TEST_NAME_SIZE - 1 && src[i] != '\0'; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

static void test_record(VnProtocolTestResult *result,
                        const char *name,
                        int passed)
{
    result->total++;
    if (passed)
    {
        result->passed++;
        return;
    }

    result->failed++;
    if (result->first_failed[0] == '\0')
        test_copy_name(result->first_failed, name);
}

static int test_bytes_equal(const VnU8 *left, const VnU8 *right, int length)
{
    int i;

    for (i = 0; i < length; i++)
    {
        if (left[i] != right[i])
            return 0;
    }
    return 1;
}

static int test_build_vector_payload(VnU16 *payload_length)
{
    VnU16 offset;
    int ok;

    offset = 0;
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           VN_TLV_NODE_NAME,
                           "IIGS");
    *payload_length = offset;
    return ok;
}

void vn_protocol_test_run(VnProtocolTestResult *result)
{
    VnU8 tiny[8];
    VnU16 offset;
    VnU16 packet_length;
    VnU16 payload_length;
    VnU16 tlv_offset;
    int receive_length;
    int extract_result;
    int tlv_result;
    int ok;

    if (result == 0)
        return;

    test_clear_result(result);

    vn_write_u16_le(tiny, 0xBEEF);
    test_record(result, "u16 little endian",
                tiny[0] == 0xEF &&
                tiny[1] == 0xBE &&
                vn_read_u16_le(tiny) == 0xBEEF);

    vn_write_u32_le(tiny, 0x1234ABCDL);
    test_record(result, "u32 little endian",
                tiny[0] == 0xCD &&
                tiny[1] == 0xAB &&
                tiny[2] == 0x34 &&
                tiny[3] == 0x12 &&
                vn_read_u32_le(tiny) == 0x1234ABCDL);

    test_record(result, "crc ascii",
                vn_crc16((const VnU8 *)"123456789", 9) == 0x29B1);

    offset = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           VN_TLV_NODE_NAME,
                           "IIGS");
    test_record(result, "tlv string IIGS",
                ok &&
                offset == 7 &&
                test_payload[0] == VN_TLV_NODE_NAME &&
                test_payload[1] == 4 &&
                test_payload[2] == 0 &&
                test_payload[3] == 'I' &&
                test_payload[4] == 'I' &&
                test_payload[5] == 'G' &&
                test_payload[6] == 'S');

    payload_length = 0;
    packet_length = 0;
    memset(test_packet_buffer, 0, sizeof(test_packet_buffer));
    ok = test_build_vector_payload(&payload_length);
    if (ok)
    {
        ok = vn_build_packet(test_packet_buffer,
                             VN_MAX_PACKET_SIZE,
                             VN_MSG_DISCOVERY_ANNOUNCE,
                             test_payload,
                             payload_length,
                             1,
                             VN_FLAG_NONE,
                             &packet_length) == VN_ERR_NONE;
    }
    test_record(result, "exact discovery vector",
                ok &&
                packet_length == 19 &&
                test_bytes_equal(test_packet_buffer, test_vector, 19));

    memset(&test_header, 0, sizeof(test_header));
    test_record(result, "header parse",
                vn_parse_header(test_vector, &test_header) == VN_ERR_NONE &&
                test_header.msg_type == VN_MSG_DISCOVERY_ANNOUNCE &&
                test_header.payload_len == 7 &&
                test_header.seq == 1 &&
                test_header.flags == VN_FLAG_NONE &&
                test_header.crc16 == 0x477B);

    memcpy(test_receive_buffer, test_vector, 19);
    receive_length = 19;
    memset(&test_packet, 0, sizeof(test_packet));
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "complete packet extract",
                extract_result == 1 &&
                receive_length == 0 &&
                test_packet.header.msg_type == VN_MSG_DISCOVERY_ANNOUNCE &&
                test_packet.header.payload_len == 7 &&
                test_packet.header.seq == 1 &&
                test_packet.header.flags == VN_FLAG_NONE &&
                test_packet.header.crc16 == 0x477B &&
                test_bytes_equal(test_packet.payload, test_vector + 12, 7));

    memcpy(test_receive_buffer, test_vector, 8);
    receive_length = 8;
    memset(&test_packet, 0, sizeof(test_packet));
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "partial header need more",
                extract_result == 0 && receive_length == 8);

    memcpy(test_receive_buffer, test_vector, 14);
    receive_length = 14;
    memset(&test_packet, 0, sizeof(test_packet));
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "partial payload need more",
                extract_result == 0 && receive_length == 14);

    test_receive_buffer[0] = 0x99;
    test_receive_buffer[1] = 0x88;
    test_receive_buffer[2] = 0x77;
    memcpy(test_receive_buffer + 3, test_vector, 19);
    receive_length = 22;
    memset(&test_packet, 0, sizeof(test_packet));
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "leading garbage resync",
                extract_result == 1 &&
                receive_length == 0 &&
                test_packet.header.msg_type == VN_MSG_DISCOVERY_ANNOUNCE);

    test_receive_buffer[0] = 0x10;
    test_receive_buffer[1] = VN_MAGIC_1;
    receive_length = 2;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "trailing V retained",
                extract_result == 0 &&
                receive_length == 1 &&
                test_receive_buffer[0] == VN_MAGIC_1);

    test_receive_buffer[0] = 0x10;
    receive_length = 1;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "non V garbage discarded",
                extract_result == 0 && receive_length == 0);

    test_receive_buffer[0] = VN_MAGIC_1;
    test_receive_buffer[1] = VN_MAGIC_2;
    test_receive_buffer[2] = VN_MAGIC_1;
    test_receive_buffer[3] = VN_MAGIC_2;
    test_receive_buffer[4] = VN_VERSION_1;
    test_receive_buffer[5] = VN_MSG_DISCOVERY_ANNOUNCE;
    vn_write_u16_le(test_receive_buffer + 6, 0);
    vn_write_u16_le(test_receive_buffer + 8, 1);
    vn_write_u16_le(test_receive_buffer + 10, 0);
    vn_write_u16_le(test_receive_buffer + 12, 0);
    receive_length = 14;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "bad magic skipped",
                extract_result == -VN_ERR_BAD_VERSION &&
                receive_length == 12 &&
                test_receive_buffer[0] == VN_MAGIC_1 &&
                test_receive_buffer[1] == VN_MAGIC_2 &&
                test_receive_buffer[2] == VN_VERSION_1);

    memcpy(test_receive_buffer, test_vector, 19);
    test_receive_buffer[2] = 2;
    receive_length = 19;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "bad version rejected",
                extract_result == -VN_ERR_BAD_VERSION &&
                receive_length == 17);

    memcpy(test_receive_buffer, test_vector, 12);
    vn_write_u16_le(test_receive_buffer + 4, VN_MAX_PAYLOAD + 1);
    receive_length = 12;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "oversized payload rejected",
                extract_result == -VN_ERR_PAYLOAD_TOO_BIG &&
                receive_length == 10);

    memcpy(test_receive_buffer, test_vector, 19);
    test_receive_buffer[18] = 'X';
    receive_length = 19;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "bad crc rejected",
                extract_result == -VN_ERR_BAD_CRC &&
                receive_length == 17);

    memcpy(test_receive_buffer, test_vector, 19);
    memcpy(test_receive_buffer + 19, test_vector, 19);
    receive_length = 38;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    ok = extract_result == 1 && receive_length == 19;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "two packets one at time",
                ok && extract_result == 1 && receive_length == 0);

    test_payload[0] = VN_TLV_NODE_NAME;
    test_payload[1] = 4;
    test_payload[2] = 0;
    test_payload[3] = 'A';
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, 4, &tlv_offset, &test_tlv);
    test_record(result, "truncated tlv rejected",
                tlv_result < 0 && tlv_offset == 0);

    offset = 0;
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           0xEE,
                           "SKIP");
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, offset, &tlv_offset, &test_tlv);
    test_record(result, "unknown tlv skipped",
                ok &&
                tlv_result == 1 &&
                test_tlv.type == 0xEE &&
                test_tlv.length == 4 &&
                tlv_offset == offset &&
                vn_tlv_next(test_payload,
                            offset,
                            &tlv_offset,
                            &test_tlv) == 0);

    offset = 0;
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           VN_TLV_NODE_NAME,
                           "IIGS");
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, offset, &tlv_offset, &test_tlv);
    test_string[0] = 'x';
    test_string[1] = 'x';
    test_string[2] = 'x';
    vn_tlv_copy_string(&test_tlv, test_string, 3);
    test_record(result, "string copy truncates",
                ok &&
                tlv_result == 1 &&
                test_string[0] == 'I' &&
                test_string[1] == 'I' &&
                test_string[2] == '\0');

    payload_length = 0;
    packet_length = 0;
    ok = test_build_vector_payload(&payload_length);
    test_record(result, "undersized packet rejected",
                ok &&
                vn_build_packet(test_packet_buffer,
                                VN_HEADER_SIZE,
                                VN_MSG_DISCOVERY_ANNOUNCE,
                                test_payload,
                                payload_length,
                                1,
                                VN_FLAG_NONE,
                                &packet_length) == VN_ERR_PAYLOAD_TOO_BIG);
}
