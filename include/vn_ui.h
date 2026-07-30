#ifndef VN_UI_H
#define VN_UI_H

#include "include/vn_config.h"

#define VN_SERIAL_DIAG_DISPLAY_BYTES 16U

typedef enum VnSerialDiagView {
    VN_SERIAL_DIAG_COUNTERS = 0,
    VN_SERIAL_DIAG_PROBE,
    VN_SERIAL_DIAG_IO
} VnSerialDiagView;

typedef struct VnSerialDiagnosticsDisplay {
    VnSerialDiagView view;
    int probe_in_flight;
    int mode_bits_active;
    int mode_bits_in_flight;
    const char *io_mode;
    const char *io_status;
    unsigned int io_requested;
    unsigned int io_accepted;
    unsigned int io_polls;
    unsigned int io_byte_count;
    unsigned int io_failure_index;
    unsigned int io_failure_value;
    const char *io_restore_status;
    unsigned char io_bytes[VN_SERIAL_DIAG_DISPLAY_BYTES];
    unsigned long rx_baseline;
    unsigned long tx_baseline;
} VnSerialDiagnosticsDisplay;

void vn_ui_draw_shell(const VnConfig *config,
                      const VnConfigStatus *status,
                      int serial_configured,
                      int serial_backend_enabled);
void vn_ui_draw_serial_diagnostics(long baud,
                                   int serial_configured,
                                   int serial_backend_enabled,
                                   const VnSerialDiagnosticsDisplay *display);

#endif
