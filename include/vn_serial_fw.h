#ifndef VN_SERIAL_FW_H
#define VN_SERIAL_FW_H

typedef struct VnSerialFirmwareResult {
    unsigned int a;
    unsigned int x;
    unsigned int y;
    unsigned int carry;
    unsigned int arbiter_error;
} VnSerialFirmwareResult;

typedef struct VnSerialModeCallResult {
    VnSerialFirmwareResult firmware;
    unsigned int result_code;
    unsigned long mode_bits;
    unsigned int dispatch_offset;
    unsigned int dispatch_address;
    unsigned int checkpoint;
    unsigned int pointer_a;
    unsigned int pointer_x;
    unsigned int pointer_y;
    unsigned int command_word0;
    unsigned int command_word1;
    unsigned int command_word2;
    unsigned int command_word3;
    unsigned int processor_status;
    unsigned int data_bank;
    unsigned int direct_page;
    unsigned int saved_stack;
} VnSerialModeCallResult;

void vn_serial_fw_init(VnSerialFirmwareResult *result);
void vn_serial_fw_init_slot(unsigned int slot,
                            VnSerialFirmwareResult *result);
void vn_serial_fw_probe_init_fwentry(VnSerialFirmwareResult *result);
void vn_serial_fw_read(VnSerialFirmwareResult *result);
void vn_serial_fw_write(unsigned char value,
                        VnSerialFirmwareResult *result);
void vn_serial_fw_write_slot(unsigned int slot,
                             unsigned char value,
                             VnSerialFirmwareResult *result);
void vn_serial_fw_status(unsigned char request,
                         VnSerialFirmwareResult *result);
void vn_serial_fw_status_slot(unsigned int slot,
                              unsigned char request,
                              VnSerialFirmwareResult *result);
void vn_serial_fw_probe_native(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_emulation(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter_query(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter_e1(VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter_value(unsigned int request,
                                      VnSerialFirmwareResult *result);
void vn_serial_fw_probe_arbiter_request(unsigned int request,
                                        VnSerialFirmwareResult *result);
void vn_serial_fw_mode_locate(VnSerialModeCallResult *result);
void vn_serial_fw_get_mode_bits(VnSerialModeCallResult *result);
void vn_serial_fw_set_mode_bits(unsigned long mode_bits,
                                VnSerialModeCallResult *result);
void vn_serial_fw_set_mode_bit23_atomic(VnSerialModeCallResult *result);
void vn_serial_fw_clear_mode_bit23_atomic(VnSerialModeCallResult *result);

#endif
