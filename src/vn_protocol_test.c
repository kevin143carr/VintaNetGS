#include <stdio.h>
#include <string.h>

#include "include/vn_protocol.h"
#include "include/vn_tlv.h"
#include "include/vn_config.h"
#include "include/vn_discovery.h"
#include "include/vn_info.h"
#include "include/vn_protocol_test.h"

#define TEST_INFO_REVISION 0x01020304UL
#define TEST_DISCOVERY_SESSION_ID 0xA1B2C3D4UL
#define TEST_DISCOVERY_SEQ 0x55AAU
#define TEST_REQUEST_ID 0x2244U
#define TEST_TARGET_NAME "MELBA"

static VnU8 test_payload[VN_MAX_PAYLOAD];
static VnU8 test_packet_buffer[VN_MAX_PACKET_SIZE];
static VnU8 test_receive_buffer[VN_MAX_PACKET_SIZE * 2];
static VnPacket test_packet;
static VnHeader test_header;
static VnTlv test_tlv;
static VnNodeInfo test_node_info;
static VnConfig test_config;
static char test_string[16];
static char test_programs[64];
static char test_line[VN_MAX_PACKET_SIZE * 2 + 80];
static VnProtocolTestLineFn test_line_fn;
static void *test_line_context;

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
        if (test_line_fn != 0)
        {
            sprintf(test_line, "TEST %s PASS", name);
            test_line_fn(test_line, test_line_context);
        }
        return;
    }

    result->failed++;
    if (result->first_failed[0] == '\0')
        test_copy_name(result->first_failed, name);
    if (test_line_fn != 0)
    {
        sprintf(test_line, "TEST %s FAIL", name);
        test_line_fn(test_line, test_line_context);
    }
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

static void test_config_init_discovery(void)
{
    memset(&test_config, 0, sizeof(test_config));
    strcpy(test_config.machine, "IIGS");
    strcpy(test_config.role, "ADMIN");
    strcpy(test_config.zmexe, "ZMODEM");
    strcpy(test_config.capabilities[0].name, "HELLO");
    strcpy(test_config.capabilities[0].command, "HELLO");
    test_config.capability_count = 1;
    test_config.baud = 2400L;
}

int vn_protocol_test_build_discovery_announce(VnU8 *packet,
                                              VnU16 packet_capacity,
                                              VnU16 *packet_length)
{
    VnU16 payload_length;

    if (packet == 0 || packet_length == 0)
        return 0;

    test_config_init_discovery();
    test_config.node_id = 0x1234U;
    payload_length = 0;
    if (!vn_build_discovery_announce(&test_config,
                                     TEST_INFO_REVISION,
                                     TEST_DISCOVERY_SESSION_ID,
                                     TEST_DISCOVERY_SEQ,
                                     test_payload,
                                     VN_MAX_PAYLOAD,
                                     &payload_length))
        return 0;
    return vn_build_packet(packet,
                           packet_capacity,
                           VN_MSG_DISCOVERY_ANNOUNCE,
                           test_payload,
                           payload_length,
                           1,
                           VN_FLAG_NONE,
                           packet_length) == VN_ERR_NONE;
}

static void test_emit_line(const char *line)
{
    if (test_line_fn != 0)
        test_line_fn(line, test_line_context);
}

static void test_emit_vector(const char *name,
                             const VnU8 *data,
                             VnU16 length)
{
    VnU16 i;
    char *cursor;

    if (test_line_fn == 0)
        return;

    cursor = test_line;
    cursor += sprintf(cursor, "VECTOR %s ", name);
    for (i = 0; i < length; i++)
        cursor += sprintf(cursor, "%02X", (unsigned int)data[i]);
    test_line_fn(test_line, test_line_context);
}

void vn_protocol_test_run_emit(VnProtocolTestResult *result,
                               VnProtocolTestLineFn line_fn,
                               void *context)
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

    test_line_fn = line_fn;
    test_line_context = context;
    test_clear_result(result);

    vn_write_u16_le(tiny, 0xBEEF);
    test_record(result, "u16_little_endian",
                tiny[0] == 0xEF &&
                tiny[1] == 0xBE &&
                vn_read_u16_le(tiny) == 0xBEEF);

    vn_write_u32_le(tiny, 0x1234ABCDL);
    test_record(result, "u32_little_endian",
                tiny[0] == 0xCD &&
                tiny[1] == 0xAB &&
                tiny[2] == 0x34 &&
                tiny[3] == 0x12 &&
                vn_read_u32_le(tiny) == 0x1234ABCDL);

    test_record(result, "crc_ascii",
                vn_crc16((const VnU8 *)"123456789", 9) == 0x29B1);

    offset = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           VN_TLV_NODE_NAME,
                           "IIGS");
    test_record(result, "tlv_string_iigs",
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
    test_record(result, "exact_discovery_vector",
                ok &&
                packet_length == 19 &&
                test_bytes_equal(test_packet_buffer, test_vector, 19));

    memset(&test_header, 0, sizeof(test_header));
    test_record(result, "header_parse",
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
    test_record(result, "complete_packet_extract",
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
    test_record(result, "partial_header_need_more",
                extract_result == 0 && receive_length == 8);

    memcpy(test_receive_buffer, test_vector, 14);
    receive_length = 14;
    memset(&test_packet, 0, sizeof(test_packet));
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "partial_payload_need_more",
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
    test_record(result, "leading_garbage_resync",
                extract_result == 1 &&
                receive_length == 0 &&
                test_packet.header.msg_type == VN_MSG_DISCOVERY_ANNOUNCE);

    test_receive_buffer[0] = 0x10;
    test_receive_buffer[1] = VN_MAGIC_1;
    receive_length = 2;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "trailing_v_retained",
                extract_result == 0 &&
                receive_length == 1 &&
                test_receive_buffer[0] == VN_MAGIC_1);

    test_receive_buffer[0] = 0x10;
    receive_length = 1;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "non_v_garbage_discarded",
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
    test_record(result, "bad_magic_skipped",
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
    test_record(result, "bad_version_rejected",
                extract_result == -VN_ERR_BAD_VERSION &&
                receive_length == 17);

    memcpy(test_receive_buffer, test_vector, 12);
    vn_write_u16_le(test_receive_buffer + 4, VN_MAX_PAYLOAD + 1);
    receive_length = 12;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "oversized_payload_rejected",
                extract_result == -VN_ERR_PAYLOAD_TOO_BIG &&
                receive_length == 10);

    memcpy(test_receive_buffer, test_vector, 19);
    test_receive_buffer[18] = 'X';
    receive_length = 19;
    extract_result = vn_extract_packet(test_receive_buffer,
                                       &receive_length,
                                       &test_packet);
    test_record(result, "bad_crc_rejected",
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
    test_record(result, "two_packets_one_at_time",
                ok && extract_result == 1 && receive_length == 0);

    test_payload[0] = VN_TLV_NODE_NAME;
    test_payload[1] = 4;
    test_payload[2] = 0;
    test_payload[3] = 'A';
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, 4, &tlv_offset, &test_tlv);
    test_record(result, "truncated_tlv_rejected",
                tlv_result < 0 && tlv_offset == 0);

    offset = 0;
    ok = vn_tlv_add_string(test_payload,
                           VN_MAX_PAYLOAD,
                           &offset,
                           0xEE,
                           "SKIP");
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, offset, &tlv_offset, &test_tlv);
    test_record(result, "unknown_tlv_skipped",
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
    test_record(result, "string_copy_truncates",
                ok &&
                tlv_result == 1 &&
                test_string[0] == 'I' &&
                test_string[1] == 'I' &&
                test_string[2] == '\0');

    payload_length = 0;
    packet_length = 0;
    ok = test_build_vector_payload(&payload_length);
    test_record(result, "undersized_packet_rejected",
                ok &&
                vn_build_packet(test_packet_buffer,
                                VN_HEADER_SIZE,
                                VN_MSG_DISCOVERY_ANNOUNCE,
                                test_payload,
                                payload_length,
                                1,
                                VN_FLAG_NONE,
                                &packet_length) == VN_ERR_PAYLOAD_TOO_BIG);

    test_config_init_discovery();
    test_record(result, "node_id_fallback",
                vn_config_node_id(&test_config) == 0xC791U);

    test_config.node_id = 0x1234U;
    test_record(result, "node_id_configured",
                vn_config_node_id(&test_config) == 0x1234U);

    test_record(result, "cap_flags_config",
                vn_config_cap_flags(&test_config) ==
                (VN_CAP_LAUNCH_PROGRAM |
                 VN_CAP_ZMODEM_SEND |
                 VN_CAP_ZMODEM_RECV));

    payload_length = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_build_discovery_announce(&test_config,
                                     TEST_INFO_REVISION,
                                     TEST_DISCOVERY_SESSION_ID,
                                     TEST_DISCOVERY_SEQ,
                                     test_payload,
                                     VN_MAX_PAYLOAD,
                                     &payload_length);
    memset(&test_node_info, 0, sizeof(test_node_info));
    test_record(result, "discovery_parse_roundtrip",
                ok &&
                vn_parse_node_info(test_payload,
                                   payload_length,
                                   &test_node_info) &&
                test_node_info.node_id == 0x1234U &&
                strcmp(test_node_info.node_name, "IIGS") == 0 &&
                strcmp(test_node_info.role, "ADMIN") == 0 &&
                test_node_info.cap_flags ==
                    (VN_CAP_LAUNCH_PROGRAM |
                     VN_CAP_ZMODEM_SEND |
                     VN_CAP_ZMODEM_RECV) &&
                test_node_info.info_revision == TEST_INFO_REVISION &&
                test_node_info.discovery_session_id ==
                    TEST_DISCOVERY_SESSION_ID &&
                test_node_info.discovery_seq == TEST_DISCOVERY_SEQ &&
                test_node_info.ttl == VN_DISCOVERY_REQUEST_TTL &&
                test_node_info.hop_count == 0);

    offset = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_tlv_add_known_node(test_payload,
                               VN_MAX_PAYLOAD,
                               &offset,
                               0x1234U,
                               "MELBA");
    tlv_offset = 0;
    tlv_result = vn_tlv_next(test_payload, offset,
                             &tlv_offset, &test_tlv);
    test_record(result, "known_node_tlv",
                ok &&
                tlv_result == 1 &&
                test_tlv.type == VN_TLV_KNOWN_NODE &&
                test_tlv.length == 7 &&
                vn_read_u16_le(test_tlv.value) == 0x1234U &&
                test_tlv.value[2] == 'M' &&
                test_tlv.value[6] == 'A');

    packet_length = 0;
    ok = vn_build_packet(test_packet_buffer,
                         VN_MAX_PACKET_SIZE,
                         VN_MSG_DISCOVERY_ANNOUNCE,
                         test_payload,
                         offset,
                         2,
                         VN_FLAG_NONE,
                         &packet_length) == VN_ERR_NONE;
    test_record(result, "discovery_packet_build",
                ok &&
                packet_length == (VnU16)(VN_HEADER_SIZE + offset) &&
                test_packet_buffer[0] == VN_MAGIC_1 &&
                test_packet_buffer[1] == VN_MAGIC_2 &&
                test_packet_buffer[3] == VN_MSG_DISCOVERY_ANNOUNCE);

    packet_length = 0;
    ok = vn_protocol_test_build_discovery_announce(test_packet_buffer,
                                                   VN_MAX_PACKET_SIZE,
                                                   &packet_length);
    test_record(result, "vector_discovery_announce",
                ok && packet_length > VN_HEADER_SIZE);
    if (ok)
        test_emit_vector("discovery_announce", test_packet_buffer,
                         packet_length);

    payload_length = 0;
    packet_length = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_build_info_req(&test_config,
                           TEST_TARGET_NAME,
                           TEST_REQUEST_ID,
                           VN_DISCOVERY_REQUEST_TTL,
                           test_payload,
                           VN_MAX_PAYLOAD,
                           &payload_length);
    if (ok)
    {
        ok = vn_build_packet(test_packet_buffer,
                             VN_MAX_PACKET_SIZE,
                             VN_MSG_INFO_REQ,
                             test_payload,
                             payload_length,
                             2,
                             VN_FLAG_NONE,
                             &packet_length) == VN_ERR_NONE;
    }
    test_record(result, "vector_info_req",
                ok && packet_length > VN_HEADER_SIZE);
    if (ok)
        test_emit_vector("info_req", test_packet_buffer, packet_length);

    payload_length = 0;
    packet_length = 0;
    memset(test_payload, 0, sizeof(test_payload));
    ok = vn_build_info_resp(&test_config,
                            TEST_TARGET_NAME,
                            TEST_INFO_REVISION,
                            TEST_REQUEST_ID,
                            test_payload,
                            VN_MAX_PAYLOAD,
                            &payload_length);
    memset(&test_node_info, 0, sizeof(test_node_info));
    test_programs[0] = '\0';
    test_record(result, "info_resp_parse_roundtrip",
                ok &&
                vn_parse_info_resp(test_payload,
                                   payload_length,
                                   &test_node_info,
                                   test_programs,
                                   sizeof(test_programs)) &&
                test_node_info.node_id == 0x1234U &&
                strcmp(test_node_info.node_name, "IIGS") == 0 &&
                strcmp(test_node_info.target_name, TEST_TARGET_NAME) == 0 &&
                strcmp(test_node_info.role, "ADMIN") == 0 &&
                test_node_info.cap_flags ==
                    (VN_CAP_LAUNCH_PROGRAM |
                     VN_CAP_ZMODEM_SEND |
                     VN_CAP_ZMODEM_RECV) &&
                test_node_info.info_revision == TEST_INFO_REVISION &&
                test_node_info.request_id == TEST_REQUEST_ID &&
                test_node_info.hop_count == 0 &&
                strcmp(test_programs, "HELLO") == 0);
    if (ok)
    {
        ok = vn_build_packet(test_packet_buffer,
                             VN_MAX_PACKET_SIZE,
                             VN_MSG_INFO_RESP,
                             test_payload,
                             payload_length,
                             3,
                             VN_FLAG_IS_RESPONSE,
                             &packet_length) == VN_ERR_NONE;
    }
    test_record(result, "vector_info_resp",
                ok && packet_length > VN_HEADER_SIZE);
    if (ok)
        test_emit_vector("info_resp", test_packet_buffer, packet_length);

    sprintf(test_line, "SUMMARY total=%d passed=%d failed=%d",
            result->total, result->passed, result->failed);
    test_emit_line(test_line);
    sprintf(test_line, "VNTEST RESULT %s",
            result->failed == 0 ? "PASS" : "FAIL");
    test_emit_line(test_line);
    test_line_fn = 0;
    test_line_context = 0;
}

void vn_protocol_test_run(VnProtocolTestResult *result)
{
    vn_protocol_test_run_emit(result, 0, 0);
}
