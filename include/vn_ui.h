#ifndef VN_UI_H
#define VN_UI_H

#include "include/vn_config.h"

void vn_ui_draw_shell(const VnConfig *config,
                      const VnConfigStatus *status,
                      int serial_configured,
                      int serial_backend_enabled);
void vn_ui_draw_serial_diagnostics(long baud,
                                   int serial_configured,
                                   int serial_backend_enabled,
                                   int probe_view,
                                   int probe_in_flight);

#endif
