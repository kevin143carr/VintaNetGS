#ifndef VN_SERIAL_FW_H
#define VN_SERIAL_FW_H

typedef struct VnSerialFirmwareResult {
    unsigned int a;
    unsigned int x;
    unsigned int y;
    unsigned int carry;
    unsigned int arbiter_error;
} VnSerialFirmwareResult;

void vn_serial_fw_init(VnSerialFirmwareResult *result);
void vn_serial_fw_read(VnSerialFirmwareResult *result);
void vn_serial_fw_write(unsigned char value,
                        VnSerialFirmwareResult *result);
void vn_serial_fw_status(unsigned char request,
                         VnSerialFirmwareResult *result);
void vn_serial_fw_probe_native(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_emulation(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter(VnSerialFirmwareResult *result);

#endif
