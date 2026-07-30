#include <string.h>

#include "include/vn_info.h"
#include "include/vn_tlv.h"

static void vn_info_append_program(char *output,
                                   int output_length,
                                   const char *name)
{
    int current_length;
    int name_length;
    int separator_length;

    current_length = strlen(output);
    name_length = strlen(name);
    separator_length = output[0] == '\0' ? 0 : 1;
    if (current_length + separator_length + name_length >= output_length)
        return;
    if (separator_length > 0)
        strcat(output, ",");
    strcat(output, name);
}

int vn_build_info_req(const VnConfig *config,
                      const char *target,
                      unsigned int request_id,
                      unsigned int ttl,
                      VnU8 *payload,
                      VnU16 payload_size,
                      VnU16 *payload_length)
{
    VnU16 offset;

    offset = 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_NODE_ID,
                        (VnU16)vn_config_node_id(config)))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset, VN_TLV_NODE_NAME,
                           config->machine))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset,
                           VN_TLV_TARGET_NAME, target))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_REQUEST_ID,
                        (VnU16)request_id))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_TTL,
                        (VnU16)ttl))
        return 0;

    *payload_length = offset;
    return 1;
}

int vn_build_info_resp(const VnConfig *config,
                       const char *target,
                       unsigned long info_revision,
                       unsigned int request_id,
                       VnU8 *payload,
                       VnU16 payload_size,
                       VnU16 *payload_length)
{
    VnU16 offset;
    int i;

    offset = 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_NODE_ID,
                        (VnU16)vn_config_node_id(config)))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset, VN_TLV_NODE_NAME,
                           config->machine))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset,
                           VN_TLV_TARGET_NAME, target))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset, VN_TLV_ROLE,
                           config->role))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset, VN_TLV_CAP_FLAGS,
                        vn_config_cap_flags(config)))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset,
                        VN_TLV_INFO_REVISION, info_revision))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_REQUEST_ID,
                        (VnU16)request_id))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset,
                        VN_TLV_HOP_COUNT, 0))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset,
                        VN_TLV_SERIAL_SPEED, (VnU32)config->baud))
        return 0;
    if (config->zmexe[0] != '\0')
    {
        if (!vn_tlv_add_string(payload, payload_size, &offset,
                               VN_TLV_FILE_PROTOCOL, "ZMODEM"))
            return 0;
    }

    for (i = 0; i < config->capability_count; i++)
    {
        if (!vn_tlv_add_string(payload, payload_size, &offset,
                               VN_TLV_PROGRAM,
                               config->capabilities[i].name))
            return 0;
    }

    *payload_length = offset;
    return 1;
}

int vn_parse_info_resp(const VnU8 *payload,
                       VnU16 payload_length,
                       VnNodeInfo *info,
                       char *programs,
                       int programs_length)
{
    VnU16 offset;
    VnTlv tlv;
    int result;
    char program[VN_CONFIG_MAX_CAPABILITY_NAME];

    offset = 0;
    memset(info, 0, sizeof(VnNodeInfo));
    if (programs_length > 0)
        programs[0] = '\0';

    while ((result = vn_tlv_next(payload, payload_length,
                                 &offset, &tlv)) == 1)
    {
        if (tlv.type == VN_TLV_NODE_ID && tlv.length == 2)
            info->node_id = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_NODE_NAME)
            vn_tlv_copy_string(&tlv, info->node_name,
                               sizeof(info->node_name));
        else if (tlv.type == VN_TLV_TARGET_NAME)
            vn_tlv_copy_string(&tlv, info->target_name,
                               sizeof(info->target_name));
        else if (tlv.type == VN_TLV_ROLE)
            vn_tlv_copy_string(&tlv, info->role, sizeof(info->role));
        else if (tlv.type == VN_TLV_CAP_FLAGS && tlv.length == 4)
            info->cap_flags = vn_read_u32_le(tlv.value);
        else if (tlv.type == VN_TLV_INFO_REVISION && tlv.length == 4)
            info->info_revision = vn_read_u32_le(tlv.value);
        else if (tlv.type == VN_TLV_REQUEST_ID && tlv.length == 2)
            info->request_id = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_TTL && tlv.length == 2)
            info->ttl = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_HOP_COUNT && tlv.length == 2)
            info->hop_count = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_PROGRAM)
        {
            vn_tlv_copy_string(&tlv, program, sizeof(program));
            vn_info_append_program(programs, programs_length, program);
        }
    }

    return result >= 0;
}
