#ifndef VN_DISCOVERY_H
#define VN_DISCOVERY_H

#include "include/vn_config.h"
#include "include/vn_protocol.h"

#define VN_DISCOVERY_REQUEST_TTL 8U

typedef struct VnNodeInfo {
    unsigned int node_id;
    char node_name[VN_CONFIG_MAX_MACHINE];
    char target_name[VN_CONFIG_MAX_MACHINE];
    char role[VN_CONFIG_MAX_ROLE];
    unsigned long cap_flags;
    unsigned long info_revision;
    unsigned long discovery_session_id;
    unsigned int discovery_seq;
    unsigned int request_id;
    unsigned int ttl;
    unsigned int hop_count;
} VnNodeInfo;

unsigned int vn_config_node_id(const VnConfig *config);
unsigned long vn_config_cap_flags(const VnConfig *config);
int vn_build_discovery_announce(const VnConfig *config,
                                unsigned long info_revision,
                                unsigned long discovery_session_id,
                                unsigned int discovery_seq,
                                VnU8 *payload,
                                VnU16 payload_size,
                                VnU16 *payload_length);
int vn_tlv_add_known_node(VnU8 *payload,
                          VnU16 payload_size,
                          VnU16 *offset,
                          unsigned int node_id,
                          const char *node_name);
int vn_build_cached_node_info(const VnNodeInfo *info,
                              const char *target,
                              VnU8 *payload,
                              VnU16 payload_size,
                              VnU16 *payload_length);
int vn_parse_node_info(const VnU8 *payload,
                       VnU16 payload_length,
                       VnNodeInfo *info);

#endif
