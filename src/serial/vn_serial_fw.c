#include <misctool.h>

#include "include/vn_serial_fw.h"

#define VN_FW_OP_INIT 0U
#define VN_FW_OP_READ 1U
#define VN_FW_OP_WRITE 2U
#define VN_FW_OP_STATUS 3U
#define VN_FW_OP_PROBE_NATIVE 4U
#define VN_FW_OP_PROBE_EMULATION 5U
#define VN_FW_OP_PROBE_ARBITER 6U

#define VN_FW_EXT_GET_MODE_BITS 1U
#define VN_FW_EXT_SET_MODE_BITS 2U
#define VN_FW_EXT_SET_BIT23_ATOMIC 3U
#define VN_FW_EXT_CLEAR_BIT23_ATOMIC 4U

#define VN_FW_SLOT_ONE_X 0x00C1U
#define VN_FW_SLOT_ONE_Y 0x0010U
#define VN_FW_SLOT_ONE_BASE 0xC100U
#define VN_FW_SLOT_ONE_EXT_OFFSET (*(volatile unsigned char *)0x00C112L)
#define VN_FW_SLOT_ONE_REQUEST 0x0001U
#define VN_FW_SLOT_ONE_PAGE 0x00C1U

#define VN_FW_SLOT_TWO_X 0x00C2U
#define VN_FW_SLOT_TWO_Y 0x0020U
#define VN_FW_SLOT_TWO_REQUEST 0x0002U
#define VN_FW_SLOT_TWO_PAGE 0x00C2U

#define VN_FW_PINIT_ENTRY (*(volatile unsigned char *)0x00C10DL)

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
unsigned int vn_fw_slot_request;
unsigned int vn_fw_slot_page;
unsigned int vn_fw_slot_number;
unsigned int vn_fw_ext_operation;
unsigned int vn_fw_ext_offset;
unsigned int vn_fw_ext_dispatch;
unsigned int vn_fw_ext_result_code;
unsigned int vn_fw_mode_in_low;
unsigned int vn_fw_mode_in_high;
unsigned int vn_fw_mode_out_low;
unsigned int vn_fw_mode_out_high;
unsigned int vn_fw_ext_checkpoint;
unsigned int vn_fw_ext_ptr_a;
unsigned int vn_fw_ext_ptr_x;
unsigned int vn_fw_ext_ptr_y;
unsigned int vn_fw_ext_cmd0;
unsigned int vn_fw_ext_cmd1;
unsigned int vn_fw_ext_cmd2;
unsigned int vn_fw_ext_cmd3;
unsigned int vn_fw_ext_status;
unsigned int vn_fw_ext_dbr;
unsigned int vn_fw_ext_dp;

extern void vn_serial_fw_invoke(void);
extern void vn_serial_fw_ext_invoke(void);
extern void vn_serial_fw_arbiter_query(void);
extern void vn_serial_fw_arbiter_query_e1(void);
extern void vn_serial_fw_arbiter_probe(void);
extern void vn_serial_fw_arbiter_request_restore(void);

static unsigned long vn_serial_fw_words_to_long(unsigned int low,
                                                unsigned int high)
{
    return ((unsigned long)low) | (((unsigned long)high) << 16);
}

static void vn_serial_fw_set_long_input(unsigned long value)
{
    vn_fw_mode_in_low = (unsigned int)(value & 0xFFFFUL);
    vn_fw_mode_in_high = (unsigned int)((value >> 16) & 0xFFFFUL);
}

static void vn_serial_fw_copy_mode_result(VnSerialModeCallResult *result)
{
    if (result != 0)
    {
        result->firmware.a = vn_fw_a_out;
        result->firmware.x = vn_fw_x_out;
        result->firmware.y = vn_fw_y_out;
        result->firmware.carry = vn_fw_carry_out;
        result->firmware.arbiter_error = vn_fw_arbiter_error;
        result->result_code = vn_fw_ext_result_code;
        result->mode_bits =
            vn_serial_fw_words_to_long(vn_fw_mode_out_low,
                                       vn_fw_mode_out_high);
        result->dispatch_offset = vn_fw_ext_offset;
        result->dispatch_address = vn_fw_ext_dispatch;
        result->checkpoint = vn_fw_ext_checkpoint;
        result->pointer_a = vn_fw_ext_ptr_a;
        result->pointer_x = vn_fw_ext_ptr_x;
        result->pointer_y = vn_fw_ext_ptr_y;
        result->command_word0 = vn_fw_ext_cmd0;
        result->command_word1 = vn_fw_ext_cmd1;
        result->command_word2 = vn_fw_ext_cmd2;
        result->command_word3 = vn_fw_ext_cmd3;
        result->processor_status = vn_fw_ext_status;
        result->data_bank = vn_fw_ext_dbr;
        result->direct_page = vn_fw_ext_dp;
        result->saved_stack = vn_fw_saved_stack;
    }
}

static void vn_serial_fw_call_slot(unsigned int slot,
                                   unsigned int operation,
                                   unsigned int a,
                                   VnSerialFirmwareResult *result)
{
    if (slot == 2U)
    {
        vn_fw_x_in = VN_FW_SLOT_TWO_X;
        vn_fw_y_in = VN_FW_SLOT_TWO_Y;
        vn_fw_slot_request = VN_FW_SLOT_TWO_REQUEST;
        vn_fw_slot_page = VN_FW_SLOT_TWO_PAGE;
        vn_fw_slot_number = 2U;
    }
    else
    {
        vn_fw_x_in = VN_FW_SLOT_ONE_X;
        vn_fw_y_in = VN_FW_SLOT_ONE_Y;
        vn_fw_slot_request = VN_FW_SLOT_ONE_REQUEST;
        vn_fw_slot_page = VN_FW_SLOT_ONE_PAGE;
        vn_fw_slot_number = 1U;
    }

    vn_fw_operation = operation;
    vn_fw_a_in = a;
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

static void vn_serial_fw_call(unsigned int operation,
                              unsigned int a,
                              unsigned int x,
                              unsigned int y,
                              VnSerialFirmwareResult *result)
{
    (void)x;
    (void)y;
    vn_serial_fw_call_slot(1U, operation, a, result);
}

void vn_serial_fw_init(VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_INIT, 0U,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_init_slot(unsigned int slot,
                            VnSerialFirmwareResult *result)
{
    vn_serial_fw_call_slot(slot, VN_FW_OP_INIT, 0U, result);
}

void vn_serial_fw_probe_init_fwentry(VnSerialFirmwareResult *result)
{
    FWRec fw_result;
    unsigned int entry;

    entry = VN_FW_SLOT_ONE_BASE | (unsigned int)VN_FW_PINIT_ENTRY;
    fw_result = FWEntry(0U, VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, entry);

    if (result != 0)
    {
        result->a = fw_result.aRegExit;
        result->x = fw_result.xRegExit;
        result->y = fw_result.yRegExit;
        result->carry = fw_result.status & 1U;
        result->arbiter_error = 0U;
    }
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

void vn_serial_fw_write_slot(unsigned int slot,
                             unsigned char value,
                             VnSerialFirmwareResult *result)
{
    vn_serial_fw_call_slot(slot, VN_FW_OP_WRITE, (unsigned int)value,
                           result);
}

void vn_serial_fw_status(unsigned char request,
                         VnSerialFirmwareResult *result)
{
    vn_serial_fw_call(VN_FW_OP_STATUS, (unsigned int)request,
                      VN_FW_SLOT_ONE_X, VN_FW_SLOT_ONE_Y, result);
}

void vn_serial_fw_status_slot(unsigned int slot,
                              unsigned char request,
                              VnSerialFirmwareResult *result)
{
    vn_serial_fw_call_slot(slot, VN_FW_OP_STATUS, (unsigned int)request,
                           result);
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

void vn_serial_fw_probe_arbiter_query(VnSerialFirmwareResult *result)
{
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;

    vn_serial_fw_arbiter_query();

    if (result != 0)
    {
        result->a = vn_fw_a_out;
        result->x = vn_fw_x_out;
        result->y = vn_fw_y_out;
        result->carry = vn_fw_carry_out;
        result->arbiter_error = vn_fw_arbiter_error;
    }
}

void vn_serial_fw_probe_arbiter_e1(VnSerialFirmwareResult *result)
{
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;

    vn_serial_fw_arbiter_query_e1();

    if (result != 0)
    {
        result->a = vn_fw_a_out;
        result->x = vn_fw_x_out;
        result->y = vn_fw_y_out;
        result->carry = vn_fw_carry_out;
        result->arbiter_error = vn_fw_arbiter_error;
    }
}

void vn_serial_fw_probe_arbiter_value(unsigned int request,
                                      VnSerialFirmwareResult *result)
{
    vn_fw_a_in = request;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;

    vn_serial_fw_arbiter_probe();

    if (result != 0)
    {
        result->a = vn_fw_a_out;
        result->x = vn_fw_x_out;
        result->y = vn_fw_y_out;
        result->carry = vn_fw_carry_out;
        result->arbiter_error = vn_fw_arbiter_error;
    }
}

void vn_serial_fw_probe_arbiter_request(unsigned int request,
                                        VnSerialFirmwareResult *result)
{
    vn_fw_a_in = request;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;

    vn_serial_fw_arbiter_request_restore();

    if (result != 0)
    {
        result->a = vn_fw_a_out;
        result->x = vn_fw_x_out;
        result->y = vn_fw_y_out;
        result->carry = vn_fw_carry_out;
        result->arbiter_error = vn_fw_arbiter_error;
    }
}

void vn_serial_fw_mode_locate(VnSerialModeCallResult *result)
{
    vn_fw_ext_offset = (unsigned int)VN_FW_SLOT_ONE_EXT_OFFSET;
    vn_fw_ext_dispatch = VN_FW_SLOT_ONE_BASE + vn_fw_ext_offset;
    vn_fw_ext_result_code = 0U;
    vn_fw_mode_out_low = 0U;
    vn_fw_mode_out_high = 0U;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;
    vn_fw_ext_checkpoint = 0U;
    vn_serial_fw_copy_mode_result(result);
}

void vn_serial_fw_get_mode_bits(VnSerialModeCallResult *result)
{
    vn_fw_ext_operation = VN_FW_EXT_GET_MODE_BITS;
    vn_fw_ext_result_code = 0U;
    vn_fw_mode_out_low = 0U;
    vn_fw_mode_out_high = 0U;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;
    vn_fw_ext_checkpoint = 0U;

    vn_serial_fw_ext_invoke();
    vn_serial_fw_copy_mode_result(result);
}

void vn_serial_fw_set_mode_bits(unsigned long mode_bits,
                                VnSerialModeCallResult *result)
{
    vn_serial_fw_set_long_input(mode_bits);
    vn_fw_ext_operation = VN_FW_EXT_SET_MODE_BITS;
    vn_fw_ext_result_code = 0U;
    vn_fw_mode_out_low = 0U;
    vn_fw_mode_out_high = 0U;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;
    vn_fw_ext_checkpoint = 0U;

    vn_serial_fw_ext_invoke();
    vn_serial_fw_copy_mode_result(result);
}

void vn_serial_fw_set_mode_bit23_atomic(VnSerialModeCallResult *result)
{
    vn_fw_ext_operation = VN_FW_EXT_SET_BIT23_ATOMIC;
    vn_fw_ext_result_code = 0U;
    vn_fw_mode_out_low = 0U;
    vn_fw_mode_out_high = 0U;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;
    vn_fw_ext_checkpoint = 0U;

    vn_serial_fw_ext_invoke();
    vn_serial_fw_copy_mode_result(result);
}

void vn_serial_fw_clear_mode_bit23_atomic(VnSerialModeCallResult *result)
{
    vn_fw_ext_operation = VN_FW_EXT_CLEAR_BIT23_ATOMIC;
    vn_fw_ext_result_code = 0U;
    vn_fw_mode_out_low = 0U;
    vn_fw_mode_out_high = 0U;
    vn_fw_a_out = 0U;
    vn_fw_x_out = 0U;
    vn_fw_y_out = 0U;
    vn_fw_carry_out = 0U;
    vn_fw_arbiter_error = 0U;
    vn_fw_ext_checkpoint = 0U;

    vn_serial_fw_ext_invoke();
    vn_serial_fw_copy_mode_result(result);
}
