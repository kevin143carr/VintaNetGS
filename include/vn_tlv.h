#ifndef VN_TLV_H
#define VN_TLV_H

#include "include/vn_protocol.h"

typedef struct VnTlv {
    VnU8 type;
    VnU16 length;
    const VnU8 *value;
} VnTlv;

int vn_tlv_add_bytes(VnU8 *buffer,
                     VnU16 buffer_size,
                     VnU16 *offset,
                     VnU8 type,
                     const VnU8 *value,
                     VnU16 length);
int vn_tlv_add_u16(VnU8 *buffer,
                   VnU16 buffer_size,
                   VnU16 *offset,
                   VnU8 type,
                   VnU16 value);
int vn_tlv_add_u32(VnU8 *buffer,
                   VnU16 buffer_size,
                   VnU16 *offset,
                   VnU8 type,
                   VnU32 value);
int vn_tlv_add_string(VnU8 *buffer,
                      VnU16 buffer_size,
                      VnU16 *offset,
                      VnU8 type,
                      const char *value);
int vn_tlv_next(const VnU8 *buffer,
                VnU16 payload_length,
                VnU16 *offset,
                VnTlv *out);
void vn_tlv_copy_string(const VnTlv *tlv, char *dest, int dest_length);

#endif
