#ifndef VN_CONFIG_UI_H
#define VN_CONFIG_UI_H

#include "include/vn_config.h"

int vn_config_ui_run_wizard(VnConfig *config, VnConfigStatus *status);
void vn_config_ui_show_startup_status(const VnConfigStatus *status);

#endif
