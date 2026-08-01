#ifndef VN_INFO_H
#define VN_INFO_H

#include "include/vn_config.h"
#include "include/vn_discovery.h"
#include "include/vn_protocol.h"

int vn_build_info_req(const VnConfig *config,
                      const char *target,
                      unsigned int request_id,
                      unsigned int ttl,
                      VnU8 *payload,
                      VnU16 payload_size,
                      VnU16 *payload_length);
int vn_build_info_resp(const VnConfig *config,
                       const char *target,
                       unsigned long info_revision,
                       unsigned int request_id,
                       VnU8 *payload,
                       VnU16 payload_size,
                       VnU16 *payload_length);
int vn_build_info_resp_basic(const VnConfig *config,
                             const char *target,
                             unsigned long info_revision,
                             unsigned int request_id,
                             VnU8 *payload,
                             VnU16 payload_size,
                             VnU16 *payload_length);
int vn_parse_info_resp(const VnU8 *payload,
                       VnU16 payload_length,
                       VnNodeInfo *info,
                       char *programs,
                       int programs_length);

#endif
