/*
 * Single-source ORCA/C workflow wrapper for VintaNetGS.
 * Canonical source remains in include/ and src/.
 */

/* TEXTUIGS implementation from ../TEXTUIGS/src/. */
segment "TUI_CORE";
#include "../TEXTUIGS/src/textui.c"
#include "../TEXTUIGS/src/textui_config.c"
#include "../TEXTUIGS/src/textui_screen.c"
#include "../TEXTUIGS/src/textui_input.c"
#include "../TEXTUIGS/src/textui_window.c"
segment "TUI_CTRL";
#include "../TEXTUIGS/src/textui_edit.c"
#include "../TEXTUIGS/src/textui_dialog.c"
#include "../TEXTUIGS/src/textui_menu.c"

/* VintaNetGS implementation from src/. */
segment "VN_PROTO";
#include "src/vn_protocol.c"
#include "src/vn_tlv.c"
#include "src/vn_protocol_test.c"
segment "VN_CONFIG";
#include "src/config/vn_config.c"
segment "VN_UI";
#include "src/ui/vn_ui.c"
#include "src/ui/vn_config_ui.c"
segment "VN_SERIAL";
#include "src/serial/vn_serial_fw.c"
#include "src/serial/vn_serial.c"
segment "VN_APP";
#include "src/vn_app.c"
#include "src/vn_main.c"

#append "src/serial/vn_serial_fw.asm"
