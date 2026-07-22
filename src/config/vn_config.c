#include <stdio.h>
#include <string.h>

#include "include/vn_config.h"

static int vn_char_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static char vn_char_to_upper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        return (char)(ch - 'a' + 'A');
    return ch;
}

static void vn_copy_string(char *dest, int dest_size, const char *src)
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

static void vn_trim(char *text)
{
    char *start;
    char *end;

    if (text == 0)
        return;

    start = text;
    while (*start != '\0' && vn_char_is_space(*start))
        start++;
    if (start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while (end > text && vn_char_is_space(*(end - 1)))
        end--;
    *end = '\0';
}

static void vn_upper_string(char *text)
{
    int i;

    if (text == 0)
        return;
    for (i = 0; text[i] != '\0'; i++)
        text[i] = vn_char_to_upper(text[i]);
}

static int vn_str_equal_ci(const char *left, const char *right)
{
    int i;

    if (left == 0 || right == 0)
        return 0;

    for (i = 0; left[i] != '\0' && right[i] != '\0'; i++)
    {
        if (vn_char_to_upper(left[i]) != vn_char_to_upper(right[i]))
            return 0;
    }
    return left[i] == '\0' && right[i] == '\0';
}

static void vn_status_set(VnConfigStatus *status,
                          VnConfigResult result,
                          int line,
                          const char *key,
                          const char *message)
{
    if (status == 0)
        return;
    status->result = result;
    status->line = line;
    vn_copy_string(status->key, sizeof(status->key), key);
    vn_copy_string(status->message, sizeof(status->message), message);
}

static int vn_parse_unsigned_long(const char *text, unsigned long *value)
{
    unsigned long result;
    int i;

    if (text == 0 || text[0] == '\0')
        return 0;

    result = 0;
    for (i = 0; text[i] != '\0'; i++)
    {
        if (text[i] < '0' || text[i] > '9')
            return 0;
        if (result > 429496729UL / 10UL)
            return 0;
        result = (result * 10UL) + (unsigned long)(text[i] - '0');
    }

    *value = result;
    return 1;
}

static int vn_parse_int_range(const char *text, int min_value, int max_value, int *value)
{
    unsigned long parsed;

    if (!vn_parse_unsigned_long(text, &parsed))
        return 0;
    if (parsed < (unsigned long)min_value || parsed > (unsigned long)max_value)
        return 0;

    *value = (int)parsed;
    return 1;
}

static int vn_parse_node_id(const char *text, unsigned int *value)
{
    unsigned long parsed;

    if (!vn_parse_unsigned_long(text, &parsed))
        return 0;
    if (parsed > 65535UL)
        return 0;

    *value = (unsigned int)parsed;
    return 1;
}

static int vn_parse_baud(const char *text, long *value)
{
    unsigned long parsed;

    if (!vn_parse_unsigned_long(text, &parsed))
        return 0;
    if (parsed != 1200UL && parsed != 2400UL && parsed != 9600UL)
        return 0;

    *value = (long)parsed;
    return 1;
}

static int vn_normalize_ports(char *ports)
{
    char output[VN_CONFIG_MAX_VALUE];
    int input_index;
    int output_index;
    int count;
    int saw_digit;
    int port;

    if (ports == 0)
        return 0;
    if (ports[0] == '\0')
        return 1;

    input_index = 0;
    output_index = 0;
    count = 0;

    while (ports[input_index] != '\0')
    {
        while (ports[input_index] == ' ' || ports[input_index] == '\t')
            input_index++;

        saw_digit = 0;
        port = 0;
        while (ports[input_index] >= '0' && ports[input_index] <= '9')
        {
            saw_digit = 1;
            port = (port * 10) + (ports[input_index] - '0');
            input_index++;
        }
        if (!saw_digit || port < 1 || port > 4)
            return 0;

        count++;
        if (count > 4 || output_index >= VN_CONFIG_MAX_VALUE - 2)
            return 0;

        if (count > 1)
        {
            output[output_index] = ',';
            output_index++;
        }
        output[output_index] = (char)('0' + port);
        output_index++;

        while (ports[input_index] == ' ' || ports[input_index] == '\t')
            input_index++;

        if (ports[input_index] == ',')
        {
            input_index++;
            if (ports[input_index] == '\0')
                return 0;
        }
        else if (ports[input_index] != '\0')
            return 0;
    }

    output[output_index] = '\0';
    vn_copy_string(ports, VN_CONFIG_MAX_VALUE, output);
    return count > 0;
}

static int vn_add_capability(VnConfig *config, const char *name, const char *command)
{
    if (config->capability_count >= VN_CONFIG_MAX_CAPABILITIES)
        return 0;

    vn_copy_string(config->capabilities[config->capability_count].name,
                   VN_CONFIG_MAX_CAPABILITY_NAME, name);
    vn_copy_string(config->capabilities[config->capability_count].command,
                   VN_CONFIG_MAX_CAPABILITY_COMMAND, command);
    config->capability_count++;
    return 1;
}

static VnConfigResult vn_apply_value(VnConfig *config,
                                     const char *key,
                                     char *value)
{
    int parsed_int;
    unsigned int parsed_node_id;
    long parsed_baud;

    if (vn_str_equal_ci(key, "MACHINE"))
    {
        vn_upper_string(value);
        vn_copy_string(config->machine, sizeof(config->machine), value);
    }
    else if (vn_str_equal_ci(key, "NODE_ID"))
    {
        if (!vn_parse_node_id(value, &parsed_node_id))
            return VN_CONFIG_INVALID_VALUE;
        config->node_id = parsed_node_id;
    }
    else if (vn_str_equal_ci(key, "ROLE"))
    {
        vn_upper_string(value);
        if (!vn_str_equal_ci(value, "ADMIN") && !vn_str_equal_ci(value, "SLAVE"))
            return VN_CONFIG_INVALID_VALUE;
        vn_copy_string(config->role, sizeof(config->role), value);
    }
    else if (vn_str_equal_ci(key, "PORTS"))
    {
        if (!vn_normalize_ports(value))
            return VN_CONFIG_INVALID_VALUE;
        vn_copy_string(config->ports, sizeof(config->ports), value);
    }
    else if (vn_str_equal_ci(key, "BAUD"))
    {
        if (!vn_parse_baud(value, &parsed_baud))
            return VN_CONFIG_INVALID_VALUE;
        config->baud = parsed_baud;
    }
    else if (vn_str_equal_ci(key, "DISCOVERY_WINDOW"))
    {
        if (!vn_parse_int_range(value, 1, 3600, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->discovery_window = parsed_int;
    }
    else if (vn_str_equal_ci(key, "DISCOVERY_REQUEST_COUNT"))
    {
        if (!vn_parse_int_range(value, 1, 99, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->discovery_request_count = parsed_int;
    }
    else if (vn_str_equal_ci(key, "DISCOVERY_REQUEST_INTERVAL_SECONDS"))
    {
        if (!vn_parse_int_range(value, 1, 3600, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->discovery_request_interval_seconds = parsed_int;
    }
    else if (vn_str_equal_ci(key, "DISCOVERY_PRESENCE_INTERVAL_SECONDS"))
    {
        if (!vn_parse_int_range(value, 1, 3600, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->discovery_presence_interval_seconds = parsed_int;
    }
    else if (vn_str_equal_ci(key, "INFO_REFRESH_SECONDS"))
    {
        if (!vn_parse_int_range(value, 1, 3600, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->info_refresh_seconds = parsed_int;
    }
    else if (vn_str_equal_ci(key, "INFO_REQUEST_COUNT"))
    {
        if (!vn_parse_int_range(value, 1, 99, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->info_request_count = parsed_int;
    }
    else if (vn_str_equal_ci(key, "INFO_REQUEST_INTERVAL_SECONDS"))
    {
        if (!vn_parse_int_range(value, 1, 3600, &parsed_int))
            return VN_CONFIG_INVALID_VALUE;
        config->info_request_interval_seconds = parsed_int;
    }
    else if (vn_str_equal_ci(key, "ZMEXE"))
    {
        vn_upper_string(value);
        vn_copy_string(config->zmexe, sizeof(config->zmexe), value);
    }
    else if (vn_str_equal_ci(key, "ZMOPTIONS"))
    {
        vn_upper_string(value);
        vn_copy_string(config->zmoptions, sizeof(config->zmoptions), value);
    }

    return VN_CONFIG_OK;
}

static VnConfigResult vn_config_set_value(VnConfig *config,
                                          const char *key,
                                          const char *value,
                                          VnConfigStatus *status)
{
    static char working_value[VN_CONFIG_MAX_VALUE];
    VnConfigResult result;

    vn_config_status_clear(status);
    if (config == 0)
    {
        vn_status_set(status, VN_CONFIG_INVALID_VALUE, 0, key,
                      "NO CONFIG STRUCT");
        return VN_CONFIG_INVALID_VALUE;
    }

    vn_copy_string(working_value, sizeof(working_value), value);
    vn_trim(working_value);
    result = vn_apply_value(config, key, working_value);
    if (result != VN_CONFIG_OK)
    {
        vn_status_set(status, result, 0, key, "INVALID CONFIG VALUE");
        return result;
    }

    vn_status_set(status, VN_CONFIG_OK, 0, key, "CONFIG VALUE OK");
    return VN_CONFIG_OK;
}

static VnConfigResult vn_config_validate_value(VnConfig *config,
                                               const char *key,
                                               const char *value,
                                               VnConfigStatus *status)
{
    VnConfigResult result;

    result = vn_config_set_value(config, key, value, status);
    if (result != VN_CONFIG_OK)
        vn_status_set(status, result, 0, key, "INVALID CONFIG VALUE");
    return result;
}

void vn_config_init_defaults(VnConfig *config)
{
    if (config == 0)
        return;

    memset(config, 0, sizeof(VnConfig));
    config->machine[0] = '\0';
    config->node_id = 0;
    vn_copy_string(config->role, sizeof(config->role), "ADMIN");
    config->ports[0] = '\0';
    config->baud = VN_CONFIG_DEFAULT_BAUD;
    config->discovery_window = VN_CONFIG_DEFAULT_DISCOVERY_WINDOW_SECONDS;
    config->discovery_request_count = VN_CONFIG_DEFAULT_DISCOVERY_REQUEST_COUNT;
    config->discovery_request_interval_seconds =
        VN_CONFIG_DEFAULT_DISCOVERY_REQUEST_INTERVAL_SECONDS;
    config->discovery_presence_interval_seconds =
        VN_CONFIG_DEFAULT_DISCOVERY_PRESENCE_INTERVAL_SECONDS;
    config->info_refresh_seconds = VN_CONFIG_DEFAULT_INFO_REFRESH_SECONDS;
    config->info_request_count = VN_CONFIG_DEFAULT_INFO_REQUEST_COUNT;
    config->info_request_interval_seconds =
        VN_CONFIG_DEFAULT_INFO_REQUEST_INTERVAL_SECONDS;
    vn_copy_string(config->zmexe, sizeof(config->zmexe), "C:\\PDZM\\ZM.EXE");
    vn_copy_string(config->zmoptions, sizeof(config->zmoptions),
                   "-NOCHAT -NOLOGO -H -D -B14400 -S -TI10 -O1");
}

void vn_config_status_clear(VnConfigStatus *status)
{
    if (status == 0)
        return;

    status->result = VN_CONFIG_OK;
    status->line = 0;
    status->key[0] = '\0';
    status->message[0] = '\0';
}

VnConfigResult vn_config_load(const char *filename,
                              VnConfig *config,
                              VnConfigStatus *status)
{
    FILE *file;
    char line[VN_CONFIG_MAX_LINE];
    char key[VN_CONFIG_MAX_VALUE];
    char value[VN_CONFIG_MAX_VALUE];
    char *equal;
    int line_number;
    int in_capabilities;
    VnConfigResult result;

    vn_config_status_clear(status);
    if (config == 0)
    {
        vn_status_set(status, VN_CONFIG_OPEN_ERROR, 0, "", "NO CONFIG STRUCT");
        return VN_CONFIG_OPEN_ERROR;
    }

    vn_config_init_defaults(config);
    if (filename == 0 || filename[0] == '\0')
        filename = VN_CONFIG_DEFAULT_FILE;

    file = fopen(filename, "r");
    if (file == 0)
    {
        vn_status_set(status, VN_CONFIG_DEFAULTS, 0, "", "CONFIG DEFAULTS");
        return VN_CONFIG_DEFAULTS;
    }

    line_number = 0;
    in_capabilities = 0;

    while (fgets(line, sizeof(line), file) != 0)
    {
        line_number++;
        vn_trim(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        if (line[0] == '[')
        {
            in_capabilities = vn_str_equal_ci(line, "[CAPABILITIES]");
            continue;
        }

        equal = strchr(line, '=');
        if (equal == 0)
        {
            fclose(file);
            vn_status_set(status, VN_CONFIG_INVALID_FORMAT, line_number,
                          "", "EXPECTED KEY = VALUE");
            return VN_CONFIG_INVALID_FORMAT;
        }

        *equal = '\0';
        vn_copy_string(key, sizeof(key), line);
        vn_copy_string(value, sizeof(value), equal + 1);
        vn_trim(key);
        vn_trim(value);
        vn_upper_string(key);

        if (key[0] == '\0')
        {
            fclose(file);
            vn_status_set(status, VN_CONFIG_INVALID_FORMAT, line_number,
                          "", "EMPTY CONFIG KEY");
            return VN_CONFIG_INVALID_FORMAT;
        }

        if (in_capabilities)
        {
            vn_upper_string(value);
            if (!vn_add_capability(config, key, value))
            {
                fclose(file);
                vn_status_set(status, VN_CONFIG_INVALID_VALUE, line_number,
                              key, "TOO MANY CAPABILITIES");
                return VN_CONFIG_INVALID_VALUE;
            }
            continue;
        }

        result = vn_apply_value(config, key, value);
        if (result != VN_CONFIG_OK)
        {
            fclose(file);
            vn_status_set(status, result, line_number, key, "INVALID CONFIG VALUE");
            return result;
        }
    }

    if (ferror(file))
    {
        fclose(file);
        vn_status_set(status, VN_CONFIG_READ_ERROR, line_number, "", "CONFIG READ ERROR");
        return VN_CONFIG_READ_ERROR;
    }

    fclose(file);
    vn_status_set(status, VN_CONFIG_OK, 0, "", "CONFIG LOADED");
    return VN_CONFIG_OK;
}

VnConfigResult vn_config_save(const char *filename,
                              const VnConfig *config,
                              VnConfigStatus *status)
{
    FILE *file;
    int i;

    vn_config_status_clear(status);
    if (config == 0)
    {
        vn_status_set(status, VN_CONFIG_WRITE_ERROR, 0, "", "NO CONFIG STRUCT");
        return VN_CONFIG_WRITE_ERROR;
    }
    if (filename == 0 || filename[0] == '\0')
        filename = VN_CONFIG_DEFAULT_FILE;

    file = fopen(filename, "w");
    if (file == 0)
    {
        vn_status_set(status, VN_CONFIG_WRITE_ERROR, 0, "", "CONFIG WRITE ERROR");
        return VN_CONFIG_WRITE_ERROR;
    }

    fprintf(file, "MACHINE = %s\n", config->machine);
    fprintf(file, "NODE_ID = %u\n", config->node_id);
    fprintf(file, "ROLE = %s\n", config->role);
    fprintf(file, "PORTS = %s\n", config->ports);
    fprintf(file, "BAUD = %ld\n", config->baud);
    fprintf(file, "DISCOVERY_WINDOW = %d\n", config->discovery_window);
    fprintf(file, "DISCOVERY_REQUEST_COUNT = %d\n",
            config->discovery_request_count);
    fprintf(file, "DISCOVERY_REQUEST_INTERVAL_SECONDS = %d\n",
            config->discovery_request_interval_seconds);
    fprintf(file, "DISCOVERY_PRESENCE_INTERVAL_SECONDS = %d\n",
            config->discovery_presence_interval_seconds);
    fprintf(file, "INFO_REFRESH_SECONDS = %d\n", config->info_refresh_seconds);
    fprintf(file, "INFO_REQUEST_COUNT = %d\n", config->info_request_count);
    fprintf(file, "INFO_REQUEST_INTERVAL_SECONDS = %d\n",
            config->info_request_interval_seconds);
    fprintf(file, "ZMEXE = %s\n", config->zmexe);
    fprintf(file, "ZMOPTIONS = %s\n", config->zmoptions);

    fprintf(file, "\n[CAPABILITIES]\n");
    for (i = 0; i < config->capability_count; i++)
    {
        fprintf(file, "%s = %s\n",
                config->capabilities[i].name,
                config->capabilities[i].command);
    }

    if (fclose(file) != 0)
    {
        vn_status_set(status, VN_CONFIG_WRITE_ERROR, 0, "", "CONFIG CLOSE ERROR");
        return VN_CONFIG_WRITE_ERROR;
    }

    vn_status_set(status, VN_CONFIG_OK, 0, "", "CONFIG SAVED");
    return VN_CONFIG_OK;
}

VnConfigResult vn_config_set_machine(VnConfig *config,
                                     const char *value,
                                     VnConfigStatus *status)
{
    return vn_config_set_value(config, "MACHINE", value, status);
}

VnConfigResult vn_config_set_role(VnConfig *config,
                                  const char *value,
                                  VnConfigStatus *status)
{
    return vn_config_set_value(config, "ROLE", value, status);
}

VnConfigResult vn_config_set_ports(VnConfig *config,
                                   const char *value,
                                   VnConfigStatus *status)
{
    return vn_config_set_value(config, "PORTS", value, status);
}

VnConfigResult vn_config_set_baud_text(VnConfig *config,
                                       const char *value,
                                       VnConfigStatus *status)
{
    return vn_config_set_value(config, "BAUD", value, status);
}

VnConfigResult vn_config_validate(const VnConfig *config,
                                  VnConfigStatus *status)
{
    static VnConfig validation_config;
    static char value[VN_CONFIG_MAX_VALUE];
    VnConfigResult result;

    vn_config_status_clear(status);
    if (config == 0)
    {
        vn_status_set(status, VN_CONFIG_INVALID_VALUE, 0, "",
                      "NO CONFIG STRUCT");
        return VN_CONFIG_INVALID_VALUE;
    }

    validation_config = *config;

    result = vn_config_validate_value(&validation_config, "MACHINE",
                                      config->machine, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%u", config->node_id);
    result = vn_config_validate_value(&validation_config, "NODE_ID",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    result = vn_config_validate_value(&validation_config, "ROLE",
                                      config->role, status);
    if (result != VN_CONFIG_OK)
        return result;

    result = vn_config_validate_value(&validation_config, "PORTS",
                                      config->ports, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%ld", config->baud);
    result = vn_config_validate_value(&validation_config, "BAUD",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->discovery_window);
    result = vn_config_validate_value(&validation_config, "DISCOVERY_WINDOW",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->discovery_request_count);
    result = vn_config_validate_value(&validation_config,
                                      "DISCOVERY_REQUEST_COUNT",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->discovery_request_interval_seconds);
    result = vn_config_validate_value(&validation_config,
                                      "DISCOVERY_REQUEST_INTERVAL_SECONDS",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->discovery_presence_interval_seconds);
    result = vn_config_validate_value(&validation_config,
                                      "DISCOVERY_PRESENCE_INTERVAL_SECONDS",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->info_refresh_seconds);
    result = vn_config_validate_value(&validation_config,
                                      "INFO_REFRESH_SECONDS",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->info_request_count);
    result = vn_config_validate_value(&validation_config,
                                      "INFO_REQUEST_COUNT",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    sprintf(value, "%d", config->info_request_interval_seconds);
    result = vn_config_validate_value(&validation_config,
                                      "INFO_REQUEST_INTERVAL_SECONDS",
                                      value, status);
    if (result != VN_CONFIG_OK)
        return result;

    result = vn_config_validate_value(&validation_config, "ZMEXE",
                                      config->zmexe, status);
    if (result != VN_CONFIG_OK)
        return result;

    result = vn_config_validate_value(&validation_config, "ZMOPTIONS",
                                      config->zmoptions, status);
    if (result != VN_CONFIG_OK)
        return result;

    if (config->capability_count < 0 ||
        config->capability_count > VN_CONFIG_MAX_CAPABILITIES)
    {
        vn_status_set(status, VN_CONFIG_INVALID_VALUE, 0, "CAPABILITIES",
                      "INVALID CAPABILITY COUNT");
        return VN_CONFIG_INVALID_VALUE;
    }

    vn_status_set(status, VN_CONFIG_OK, 0, "", "CONFIG VALID");
    return VN_CONFIG_OK;
}

const char *vn_config_result_text(VnConfigResult result)
{
    switch (result)
    {
        case VN_CONFIG_OK:
            return "CONFIG LOADED";
        case VN_CONFIG_DEFAULTS:
            return "CONFIG DEFAULTS";
        case VN_CONFIG_OPEN_ERROR:
            return "CONFIG OPEN ERROR";
        case VN_CONFIG_READ_ERROR:
            return "CONFIG READ ERROR";
        case VN_CONFIG_INVALID_VALUE:
            return "CONFIG ERROR";
        case VN_CONFIG_INVALID_FORMAT:
            return "CONFIG ERROR";
        case VN_CONFIG_WRITE_ERROR:
            return "CONFIG WRITE ERROR";
    }
    return "CONFIG ERROR";
}

int vn_config_needs_setup(const VnConfig *config)
{
    if (config == 0)
        return 1;
    return config->machine[0] == '\0' || config->ports[0] == '\0';
}
