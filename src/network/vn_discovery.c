#include <string.h>

#include "include/vn_discovery.h"
#include "include/vn_tlv.h"

unsigned int vn_config_node_id(const VnConfig *config)
{
    unsigned int hash;
    int i;

    if (config->node_id != 0)
        return config->node_id;

    hash = 5381U;
    for (i = 0; config->machine[i] != '\0'; i++)
        hash = (unsigned int)(((hash << 5) + hash) ^
                              (unsigned char)config->machine[i]);
    if (hash == 0)
        hash = 1U;
    return hash;
}

unsigned long vn_config_cap_flags(const VnConfig *config)
{
    unsigned long flags;

    flags = 0;
    if (config->capability_count > 0)
        flags |= VN_CAP_LAUNCH_PROGRAM;
    if (config->zmexe[0] != '\0')
        flags |= VN_CAP_ZMODEM_SEND | VN_CAP_ZMODEM_RECV;
    return flags;
}

int vn_build_discovery_announce(const VnConfig *config,
                                unsigned long info_revision,
                                unsigned long discovery_session_id,
                                unsigned int discovery_seq,
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
    if (!vn_tlv_add_string(payload, payload_size, &offset, VN_TLV_ROLE,
                           config->role))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset, VN_TLV_CAP_FLAGS,
                        vn_config_cap_flags(config)))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset,
                        VN_TLV_INFO_REVISION, info_revision))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset,
                        VN_TLV_DISCOVERY_SESSION_ID,
                        discovery_session_id))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset,
                        VN_TLV_DISCOVERY_SEQ,
                        (VnU16)discovery_seq))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_TTL,
                        VN_DISCOVERY_REQUEST_TTL))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset,
                        VN_TLV_HOP_COUNT, 0))
        return 0;

    *payload_length = offset;
    return 1;
}

int vn_tlv_add_known_node(VnU8 *payload,
                          VnU16 payload_size,
                          VnU16 *offset,
                          unsigned int node_id,
                          const char *node_name)
{
    VnU8 value[2 + VN_CONFIG_MAX_MACHINE];
    VnU16 name_length;

    if (node_name == 0)
        node_name = "";
    name_length = (VnU16)strlen(node_name);
    if (name_length > VN_CONFIG_MAX_MACHINE - 1)
        name_length = VN_CONFIG_MAX_MACHINE - 1;

    vn_write_u16_le(value, (VnU16)node_id);
    if (name_length > 0)
        memcpy(value + 2, node_name, name_length);
    return vn_tlv_add_bytes(payload, payload_size, offset,
                            VN_TLV_KNOWN_NODE, value,
                            (VnU16)(2 + name_length));
}

int vn_build_cached_node_info(const VnNodeInfo *info,
                              const char *target,
                              VnU8 *payload,
                              VnU16 payload_size,
                              VnU16 *payload_length)
{
    VnU16 offset;

    offset = 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_NODE_ID,
                        (VnU16)info->node_id))
        return 0;
    if (!vn_tlv_add_string(payload, payload_size, &offset, VN_TLV_NODE_NAME,
                           info->node_name))
        return 0;
    if (target != 0 && target[0] != '\0')
    {
        if (!vn_tlv_add_string(payload, payload_size, &offset,
                               VN_TLV_TARGET_NAME, target))
            return 0;
    }
    if (info->role[0] != '\0')
    {
        if (!vn_tlv_add_string(payload, payload_size, &offset,
                               VN_TLV_ROLE, info->role))
            return 0;
    }
    if (!vn_tlv_add_u32(payload, payload_size, &offset, VN_TLV_CAP_FLAGS,
                        info->cap_flags))
        return 0;
    if (!vn_tlv_add_u32(payload, payload_size, &offset,
                        VN_TLV_INFO_REVISION, info->info_revision))
        return 0;
    if (info->discovery_session_id != 0)
    {
        if (!vn_tlv_add_u32(payload, payload_size, &offset,
                            VN_TLV_DISCOVERY_SESSION_ID,
                            info->discovery_session_id))
            return 0;
    }
    if (info->discovery_seq != 0)
    {
        if (!vn_tlv_add_u16(payload, payload_size, &offset,
                            VN_TLV_DISCOVERY_SEQ,
                            (VnU16)info->discovery_seq))
            return 0;
    }
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_REQUEST_ID,
                        (VnU16)info->request_id))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_TTL,
                        (VnU16)info->ttl))
        return 0;
    if (!vn_tlv_add_u16(payload, payload_size, &offset, VN_TLV_HOP_COUNT,
                        (VnU16)info->hop_count))
        return 0;

    *payload_length = offset;
    return 1;
}

int vn_parse_node_info(const VnU8 *payload,
                       VnU16 payload_length,
                       VnNodeInfo *info)
{
    VnU16 offset;
    VnTlv tlv;
    int result;

    offset = 0;
    memset(info, 0, sizeof(VnNodeInfo));
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
        else if (tlv.type == VN_TLV_DISCOVERY_SESSION_ID &&
                 tlv.length == 4)
            info->discovery_session_id = vn_read_u32_le(tlv.value);
        else if (tlv.type == VN_TLV_DISCOVERY_SEQ && tlv.length == 2)
            info->discovery_seq = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_REQUEST_ID && tlv.length == 2)
            info->request_id = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_TTL && tlv.length == 2)
            info->ttl = vn_read_u16_le(tlv.value);
        else if (tlv.type == VN_TLV_HOP_COUNT && tlv.length == 2)
            info->hop_count = vn_read_u16_le(tlv.value);
    }

    return result >= 0;
}
