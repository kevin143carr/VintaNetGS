#include <stdio.h>
#include <string.h>

#include "include/textui_dialog.h"
#include "include/textui_edit.h"
#include "include/vn_config.h"
#include "include/vn_config_ui.h"

static VnConfig wizard_config;
static VnConfigStatus wizard_status;
static char edit_buffer[TEXTUI_EDIT_MAX_CAPACITY];
static char message_buffer[VN_CONFIG_MAX_VALUE];

static const char *role_items[] = {
    "ADMIN",
    "SLAVE"
};

static const char *baud_items[] = {
    "1200",
    "2400",
    "9600"
};

static void wizard_copy_string(char *dest, int dest_size, const char *src)
{
    int i;

    if (dest == 0 || dest_size <= 0)
        return;
    if (src == 0)
        src = "";

    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

static void wizard_show_cancel(void)
{
    textui_message_dialog("CONFIG CANCELED",
                          "No configuration changes were saved.");
}

static void wizard_show_status_message(const char *title,
                                       const char *field,
                                       const VnConfigStatus *status)
{
    wizard_copy_string(message_buffer, sizeof(message_buffer), field);
    wizard_copy_string(message_buffer + strlen(message_buffer),
                       (int)(sizeof(message_buffer) - strlen(message_buffer)),
                       ": ");
    if (status != 0 && status->message[0] != '\0')
    {
        wizard_copy_string(message_buffer + strlen(message_buffer),
                           (int)(sizeof(message_buffer) -
                                 strlen(message_buffer)),
                           status->message);
    }
    else
    {
        wizard_copy_string(message_buffer + strlen(message_buffer),
                           (int)(sizeof(message_buffer) -
                                 strlen(message_buffer)),
                           "Invalid value.");
    }
    textui_message_dialog(title, message_buffer);
}

static int wizard_ports_filter(unsigned char ch, void *context)
{
    (void)context;
    if (ch >= '0' && ch <= '9')
        return 1;
    if (ch == ',' || ch == ' ' || ch == '\t')
        return 1;
    return 0;
}

static unsigned int wizard_role_index(const VnConfig *config)
{
    if (config != 0 && strcmp(config->role, "SLAVE") == 0)
        return 1;
    return 0;
}

static unsigned int wizard_baud_index(const VnConfig *config)
{
    if (config != 0)
    {
        if (config->baud == 1200L)
            return 0;
        if (config->baud == 9600L)
            return 2;
    }
    return 1;
}

static int wizard_edit_machine(void)
{
    int accepted;
    VnConfigResult result;

    wizard_copy_string(edit_buffer, sizeof(edit_buffer),
                       wizard_config.machine);
    while (1)
    {
        accepted = textui_edit_dialog("MACHINE",
                                      "Machine name:",
                                      edit_buffer,
                                      VN_CONFIG_MAX_MACHINE,
                                      24,
                                      TEXTUI_EDIT_FLAG_UPPERCASE,
                                      textui_edit_filter_printable,
                                      0);
        if (!accepted)
            return 0;

        result = vn_config_set_machine(&wizard_config,
                                       edit_buffer,
                                       &wizard_status);
        if (result == VN_CONFIG_OK)
            return 1;

        wizard_show_status_message("INVALID MACHINE",
                                   "MACHINE",
                                   &wizard_status);
    }
}

static int wizard_select_role(void)
{
    unsigned int selected;
    int accepted;
    VnConfigResult result;

    while (1)
    {
        selected = wizard_role_index(&wizard_config);
        accepted = textui_select_dialog("ROLE",
                                        "Select role:",
                                        role_items,
                                        sizeof(role_items) /
                                        sizeof(role_items[0]),
                                        &selected);
        if (!accepted)
            return 0;

        result = vn_config_set_role(&wizard_config,
                                    role_items[selected],
                                    &wizard_status);
        if (result == VN_CONFIG_OK)
            return 1;

        wizard_show_status_message("INVALID ROLE",
                                   "ROLE",
                                   &wizard_status);
    }
}

static int wizard_edit_ports(void)
{
    int accepted;
    VnConfigResult result;

    wizard_copy_string(edit_buffer, sizeof(edit_buffer),
                       wizard_config.ports);
    while (1)
    {
        accepted = textui_edit_dialog("PORTS",
                                      "Ports:",
                                      edit_buffer,
                                      sizeof(edit_buffer),
                                      24,
                                      0,
                                      wizard_ports_filter,
                                      0);
        if (!accepted)
            return 0;

        result = vn_config_set_ports(&wizard_config,
                                     edit_buffer,
                                     &wizard_status);
        if (result == VN_CONFIG_OK)
            return 1;

        wizard_show_status_message("INVALID PORTS",
                                   "PORTS",
                                   &wizard_status);
    }
}

static int wizard_select_baud(void)
{
    unsigned int selected;
    int accepted;
    VnConfigResult result;

    while (1)
    {
        selected = wizard_baud_index(&wizard_config);
        accepted = textui_select_dialog("BAUD",
                                        "Select baud:",
                                        baud_items,
                                        sizeof(baud_items) /
                                        sizeof(baud_items[0]),
                                        &selected);
        if (!accepted)
            return 0;

        result = vn_config_set_baud_text(&wizard_config,
                                         baud_items[selected],
                                         &wizard_status);
        if (result == VN_CONFIG_OK)
            return 1;

        wizard_show_status_message("INVALID BAUD",
                                   "BAUD",
                                   &wizard_status);
    }
}

int vn_config_ui_run_wizard(VnConfig *config, VnConfigStatus *status)
{
    VnConfigResult result;
    int confirmed;

    if (config == 0 || status == 0)
        return 0;

    wizard_config = *config;
    vn_config_status_clear(&wizard_status);

    if (!wizard_edit_machine())
    {
        wizard_show_cancel();
        return 0;
    }
    if (!wizard_select_role())
    {
        wizard_show_cancel();
        return 0;
    }
    if (!wizard_edit_ports())
    {
        wizard_show_cancel();
        return 0;
    }
    if (!wizard_select_baud())
    {
        wizard_show_cancel();
        return 0;
    }

    result = vn_config_validate(&wizard_config, &wizard_status);
    if (result != VN_CONFIG_OK)
    {
        wizard_show_status_message("CONFIG INVALID",
                                   wizard_status.key,
                                   &wizard_status);
        return 0;
    }

    confirmed = textui_confirm_dialog("SAVE CONFIG",
                                      "Write VINTANETGS.CFG?");
    if (!confirmed)
    {
        textui_message_dialog("CONFIG NOT SAVED",
                              "No configuration changes were saved.");
        return 0;
    }

    result = vn_config_save(VN_CONFIG_DEFAULT_FILE,
                            &wizard_config,
                            &wizard_status);
    if (result != VN_CONFIG_OK)
    {
        *status = wizard_status;
        wizard_show_status_message("SAVE FAILED",
                                   "CONFIG",
                                   &wizard_status);
        return 0;
    }

    *config = wizard_config;
    *status = wizard_status;
    textui_message_dialog("CONFIG SAVED",
                          "Configuration saved.");
    return 1;
}

void vn_config_ui_show_startup_status(const VnConfigStatus *status)
{
    if (status == 0)
        return;

    if (status->result == VN_CONFIG_DEFAULTS)
    {
        textui_message_dialog("CONFIG DEFAULTS",
                              "VINTANETGS.CFG was not found.");
    }
    else if (status->result == VN_CONFIG_INVALID_VALUE ||
             status->result == VN_CONFIG_INVALID_FORMAT ||
             status->result == VN_CONFIG_READ_ERROR ||
             status->result == VN_CONFIG_OPEN_ERROR)
    {
        wizard_show_status_message("CONFIG ERROR",
                                   status->key[0] == '\0' ?
                                   "CONFIG" : status->key,
                                   status);
    }
}
