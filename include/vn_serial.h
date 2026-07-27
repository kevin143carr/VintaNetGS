#ifndef VN_SERIAL_H
#define VN_SERIAL_H

#define VN_SERIAL_RX_BUFFER_SIZE 1024U
#define VN_SERIAL_TX_BUFFER_SIZE 1024U
#define VN_SERIAL_HISTORY_SIZE 8U
#define VN_SERIAL_PROBE_TOTAL_STEPS 39U

typedef enum VnSerialStatus {
    VN_SERIAL_STATUS_CLOSED = 0,
    VN_SERIAL_STATUS_OPEN,
    VN_SERIAL_STATUS_ERROR
} VnSerialStatus;

typedef struct VnSerialStats {
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned int rx_queued;
    unsigned int tx_queued;
    unsigned int rx_high_water;
    unsigned int tx_high_water;
    unsigned int rx_overflows;
    unsigned int tx_overflows;
    unsigned int framing_errors;
    unsigned int parity_errors;
    unsigned int overrun_errors;
    int last_error;
} VnSerialStats;

typedef enum VnSerialProbeOperation {
    VN_SERIAL_PROBE_NONE = 0,
    VN_SERIAL_PROBE_NATIVE,
    VN_SERIAL_PROBE_EMULATION,
    VN_SERIAL_PROBE_ARBITER,
    VN_SERIAL_PROBE_ARBITER_E1,
    VN_SERIAL_PROBE_ARBITER_INT1,
    VN_SERIAL_PROBE_ARBITER_EXT1,
    VN_SERIAL_PROBE_INIT,
    VN_SERIAL_PROBE_WRITE,
    VN_SERIAL_PROBE_STATUS_RX,
    VN_SERIAL_PROBE_STATUS_TX
} VnSerialProbeOperation;

typedef enum VnSerialProbeOutcome {
    VN_SERIAL_PROBE_READY = 0,
    VN_SERIAL_PROBE_IN_FLIGHT,
    VN_SERIAL_PROBE_PASSED,
    VN_SERIAL_PROBE_INFO,
    VN_SERIAL_PROBE_FAILED,
    VN_SERIAL_PROBE_COMPLETE
} VnSerialProbeOutcome;

typedef struct VnSerialProbeStatus {
    unsigned int next_step;
    unsigned int last_step;
    unsigned int total_steps;
    VnSerialProbeOperation next_operation;
    VnSerialProbeOperation last_operation;
    unsigned int next_value;
    unsigned int last_value;
    unsigned int a;
    unsigned int x;
    unsigned int y;
    unsigned int carry;
    unsigned int arbiter_error;
    VnSerialProbeOutcome outcome;
} VnSerialProbeStatus;

typedef enum VnSerialModeStage {
    VN_SERIAL_MODE_STAGE_LOCATE = 0,
    VN_SERIAL_MODE_STAGE_INIT,
    VN_SERIAL_MODE_STAGE_GET_ORIGINAL,
    VN_SERIAL_MODE_STAGE_CLEAR_BIT23,
    VN_SERIAL_MODE_STAGE_VERIFY_COMMANDS,
    VN_SERIAL_MODE_STAGE_SETUP_STATUS,
    VN_SERIAL_MODE_STAGE_SETUP_WRITE,
    VN_SERIAL_MODE_STAGE_PREPARE,
    VN_SERIAL_MODE_STAGE_SET_BIT23,
    VN_SERIAL_MODE_STAGE_VERIFY_BIT23,
    VN_SERIAL_MODE_STAGE_PAYLOAD_STATUS,
    VN_SERIAL_MODE_STAGE_PAYLOAD_WRITE,
    VN_SERIAL_MODE_STAGE_RESTORE,
    VN_SERIAL_MODE_STAGE_VERIFY_RESTORE,
    VN_SERIAL_MODE_STAGE_COMPLETE,
    VN_SERIAL_MODE_STAGE_FAILED
} VnSerialModeStage;

typedef enum VnSerialModeOutcome {
    VN_SERIAL_MODE_READY = 0,
    VN_SERIAL_MODE_IN_FLIGHT,
    VN_SERIAL_MODE_PASSED,
    VN_SERIAL_MODE_WAIT,
    VN_SERIAL_MODE_FAILED,
    VN_SERIAL_MODE_COMPLETE
} VnSerialModeOutcome;

typedef struct VnSerialModeStatus {
    unsigned int step;
    unsigned int total_steps;
    VnSerialModeStage stage;
    VnSerialModeStage failure_stage;
    VnSerialModeOutcome outcome;
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
    unsigned long original_mode_bits;
    unsigned long modified_mode_bits;
    unsigned long returned_mode_bits;
    unsigned long restored_mode_bits;
    unsigned int original_valid;
    unsigned int bit23;
    unsigned int unexpected_changes;
    unsigned int result_code;
    unsigned int carry;
    unsigned int arbiter_error;
    unsigned int setup_index;
    unsigned int setup_value;
    unsigned int setup_accepted;
    unsigned int payload_index;
    unsigned int payload_value;
    unsigned int payload_accepted;
    unsigned int restore_done;
    unsigned int configured_setup;
} VnSerialModeStatus;

int vn_serial_open(int slot, long baud);
void vn_serial_close(void);
void vn_serial_poll(void);

int vn_serial_read_byte(unsigned char *value);
unsigned int vn_serial_write(const unsigned char *data,
                             unsigned int length);
unsigned int vn_serial_diag_write_raw(const unsigned char *data,
                                      unsigned int length);
unsigned int vn_serial_diag_write_raw_printer_unconfigured(
    const unsigned char *data,
    unsigned int length);
unsigned int vn_serial_diag_write_raw_modem(long baud,
                                            const unsigned char *data,
                                            unsigned int length);
int vn_serial_diag_modem_init(void);
int vn_serial_diag_modem_status_tx(void);
int vn_serial_diag_modem_write_byte(unsigned char value);
int vn_serial_diag_printer_status_tx(void);
int vn_serial_diag_printer_write_byte(unsigned char value);

VnSerialStatus vn_serial_status(void);
const VnSerialStats *vn_serial_stats(void);
const char *vn_serial_status_text(void);

unsigned int vn_serial_recent_rx(unsigned char *data,
                                 unsigned int capacity);
unsigned int vn_serial_recent_tx(unsigned char *data,
                                 unsigned int capacity);

int vn_serial_ring_self_test(void);

void vn_serial_probe_reset(void);
int vn_serial_probe_next(void);
const VnSerialProbeStatus *vn_serial_probe_status(void);
const char *vn_serial_probe_operation_text(VnSerialProbeOperation operation);

void vn_serial_mode_reset(void);
void vn_serial_mode8n1_reset(void);
int vn_serial_mode_next(void);
int vn_serial_mode_prepare_restore(void);
int vn_serial_mode_restore_next(void);
const VnSerialModeStatus *vn_serial_mode_status(void);
const char *vn_serial_mode_stage_text(VnSerialModeStage stage);

#endif
