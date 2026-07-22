#include "include/vn_serial_fw.h"

#define VN_FW_OP_INIT 0U
#define VN_FW_OP_READ 1U
#define VN_FW_OP_WRITE 2U
#define VN_FW_OP_STATUS 3U
#define VN_FW_OP_PROBE_NATIVE 4U
#define VN_FW_OP_PROBE_EMULATION 5U
#define VN_FW_OP_PROBE_ARBITER 6U

#define VN_FW_SLOT_ONE_X 0x00C1U
#define VN_FW_SLOT_ONE_Y 0x0010U

/*
 * Shared with the bank-zero ORCA/M bridge. Calls are synchronous, so a
 * single file-scope parameter block avoids relying on the C stack while
 * the processor is temporarily in emulation mode.
 */
unsigned int vn_fw_operation;
unsigned int vn_fw_a_in;
unsigned int vn_fw_x_in;
unsigned int vn_fw_y_in;
unsigned int vn_fw_a_out;
unsigned int vn_fw_x_out;
unsigned int vn_fw_y_out;
unsigned int vn_fw_carry_out;
unsigned int vn_fw_arbiter_error;
unsigned int vn_fw_saved_slots;
unsigned int vn_fw_saved_stack;

extern void vn_serial_fw_invoke(void);

static void vn_serial_fw_call(unsigned int operation,
                              unsigned int a,
                              unsigned int x,
                              unsigned int y,
                              VnSerialFirmwareResult *result)
{
    vn_fw_operation = operation;
    vn_fw_a_in = a;
    vn_fw_x_in = x;
    vn_fw_y_in = y;
    vn_serial_fw_invoke();

    if (result != 0)
    {
        result->a = vn_fw_a_out;
        result->x = vn_fw_x_out;
        result->y = vn_fw_y_out;
        result->carry = vn_fw_carry_out;
        result->arbiter_error = vn_fw_arbiter_error;
    }
}

void vn_serial_fw_init(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_INIT, 0U,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_read(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_READ, 0U,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_write(unsigned char value,
                        VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_WRITE, (unsigned int)value,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_status(unsigned char request,
                         VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_STATUS, (unsigned int)request,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_probe_native(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_PROBE_NATIVE, 0U, 0U, 0U, result);
}

void vn_serial_fw_probe_emulation(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_PROBE_EMULATION, 0U, 0U, 0U, result);
}

void vn_serial_fw_probe_arbiter(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_PROBE_ARBITER, 0U, 0U, 0U, result);
}
