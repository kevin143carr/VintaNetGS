#ifndef VN_UI_H
#define VN_UI_H

#include "include/vn_config.h"

#define VN_UI_DASHBOARD_MACHINE_ROWS 8U
#define VN_UI_DASHBOARD_ROUTE_SIZE 12U
#define VN_UI_DASHBOARD_PORT_SIZE 8U
#define VN_UI_DASHBOARD_STATUS_SIZE 32U
#define VN_UI_DASHBOARD_CAPABILITY_SIZE VN_CONFIG_MAX_CAPABILITY_NAME

#define VN_SERIAL_DIAG_DISPLAY_BYTES 16U

typedef struct VnUiDashboardMachine {
    char machine[VN_CONFIG_MAX_MACHINE];
    char role[VN_CONFIG_MAX_ROLE];
    char port[VN_UI_DASHBOARD_PORT_SIZE];
    char route[VN_UI_DASHBOARD_ROUTE_SIZE];
    unsigned int capability_count;
    char selected_capability[VN_UI_DASHBOARD_CAPABILITY_SIZE];
} VnUiDashboardMachine;

typedef struct VnUiDashboardDisplay {
    const VnConfig *config;
    const VnConfigStatus *config_status;
    int serial_configured;
    int serial_backend_enabled;
    const char *packet_status;
    const char *status_text;
    unsigned int machine_count;
    unsigned int selected_machine;
    unsigned int selected_capability;
    VnUiDashboardMachine machines[VN_UI_DASHBOARD_MACHINE_ROWS];
} VnUiDashboardDisplay;

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
void vn_ui_draw_dashboard(const VnUiDashboardDisplay *display);
void vn_ui_update_dashboard_local(const VnUiDashboardDisplay *display);
void vn_ui_update_dashboard_machines(const VnUiDashboardDisplay *display);
void vn_ui_update_dashboard_details(const VnUiDashboardDisplay *display);
void vn_ui_update_dashboard_status(const VnUiDashboardDisplay *display);
void vn_ui_draw_serial_diagnostics(long baud,
                                   int serial_configured,
                                   int serial_backend_enabled,
                                   const VnSerialDiagnosticsDisplay *display);

#endif
