#ifndef VN_SERIAL_H
#define VN_SERIAL_H

#define VN_SERIAL_RX_BUFFER_SIZE 1024U
#define VN_SERIAL_TX_BUFFER_SIZE 1024U
#define VN_SERIAL_HISTORY_SIZE 8U
#define VN_SERIAL_PROBE_TOTAL_STEPS 37U

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
    VN_SERIAL_PROBE_INIT,
    VN_SERIAL_PROBE_WRITE,
    VN_SERIAL_PROBE_STATUS_RX,
    VN_SERIAL_PROBE_STATUS_TX
} VnSerialProbeOperation;

typedef enum VnSerialProbeOutcome {
    VN_SERIAL_PROBE_READY = 0,
    VN_SERIAL_PROBE_IN_FLIGHT,
    VN_SERIAL_PROBE_PASSED,
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

int vn_serial_open(int slot, long baud);
void vn_serial_close(void);
void vn_serial_poll(void);

int vn_serial_read_byte(unsigned char *value);
unsigned int vn_serial_write(const unsigned char *data,
                             unsigned int length);

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

#endif
