#include <string.h>

#include "include/vn_tlv.h"

int vn_tlv_add_bytes(VnU8 *buffer,
                     VnU16 buffer_size,
                     VnU16 *offset,
                     VnU8 type,
                     const VnU8 *value,
                     VnU16 length)
{
    if ((VnU32)*offset + 3L + (VnU32)length > (VnU32)buffer_size)
        return 0;

    buffer[*offset] = type;
    vn_write_u16_le(buffer + *offset + 1, length);
    *offset = (VnU16)(*offset + 3);
    if (length > 0 && value != 0)
        memcpy(buffer + *offset, value, length);
    *offset = (VnU16)(*offset + length);
    return 1;
}

int vn_tlv_add_u16(VnU8 *buffer,
                   VnU16 buffer_size,
                   VnU16 *offset,
                   VnU8 type,
                   VnU16 value)
{
    VnU8 temp[2];

    vn_write_u16_le(temp, value);
    return vn_tlv_add_bytes(buffer, buffer_size, offset, type, temp, 2);
}

int vn_tlv_add_u32(VnU8 *buffer,
                   VnU16 buffer_size,
                   VnU16 *offset,
                   VnU8 type,
                   VnU32 value)
{
    VnU8 temp[4];

    vn_write_u32_le(temp, value);
    return vn_tlv_add_bytes(buffer, buffer_size, offset, type, temp, 4);
}

int vn_tlv_add_string(VnU8 *buffer,
                      VnU16 buffer_size,
                      VnU16 *offset,
                      VnU8 type,
                      const char *value)
{
    if (value == 0)
        value = "";
    return vn_tlv_add_bytes(buffer, buffer_size, offset, type,
                            (const VnU8 *)value, (VnU16)strlen(value));
}

int vn_tlv_next(const VnU8 *buffer,
                VnU16 payload_length,
                VnU16 *offset,
                VnTlv *out)
{
    VnU16 length;

    if (*offset == payload_length)
        return 0;
    if ((VnU32)*offset + 3L > (VnU32)payload_length)
        return -1;

    out->type = buffer[*offset];
    length = vn_read_u16_le(buffer + *offset + 1);
    if ((VnU32)*offset + 3L + (VnU32)length > (VnU32)payload_length)
        return -1;

    out->length = length;
    out->value = buffer + *offset + 3;
    *offset = (VnU16)(*offset + 3 + length);
    return 1;
}

void vn_tlv_copy_string(const VnTlv *tlv, char *dest, int dest_length)
{
    int copy_length;

    if (dest_length <= 0)
        return;
    copy_length = tlv->length;
    if (copy_length > dest_length - 1)
        copy_length = dest_length - 1;
    if (copy_length > 0)
        memcpy(dest, tlv->value, copy_length);
    dest[copy_length] = '\0';
}
