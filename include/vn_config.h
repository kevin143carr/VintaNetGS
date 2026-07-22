#ifndef VN_CONFIG_H
#define VN_CONFIG_H

#define VN_CONFIG_DEFAULT_FILE "VINTANETGS.CFG"

#define VN_CONFIG_MAX_LINE 220
#define VN_CONFIG_MAX_VALUE 180
#define VN_CONFIG_MAX_MACHINE 32
#define VN_CONFIG_MAX_ROLE 12
#define VN_CONFIG_MAX_FILE 128
#define VN_CONFIG_MAX_CAPABILITIES 24
#define VN_CONFIG_MAX_CAPABILITY_NAME 32
#define VN_CONFIG_MAX_CAPABILITY_COMMAND 128

#define VN_CONFIG_DEFAULT_BAUD 2400L
#define VN_CONFIG_DEFAULT_DISCOVERY_WINDOW_SECONDS 30
#define VN_CONFIG_DEFAULT_DISCOVERY_REQUEST_COUNT 3
#define VN_CONFIG_DEFAULT_DISCOVERY_REQUEST_INTERVAL_SECONDS 5
#define VN_CONFIG_DEFAULT_DISCOVERY_PRESENCE_INTERVAL_SECONDS 30
#define VN_CONFIG_DEFAULT_INFO_REFRESH_SECONDS 60
#define VN_CONFIG_DEFAULT_INFO_REQUEST_COUNT 3
#define VN_CONFIG_DEFAULT_INFO_REQUEST_INTERVAL_SECONDS 5

typedef enum VnConfigResult {
    VN_CONFIG_OK,
    VN_CONFIG_DEFAULTS,
    VN_CONFIG_OPEN_ERROR,
    VN_CONFIG_READ_ERROR,
    VN_CONFIG_INVALID_VALUE,
    VN_CONFIG_INVALID_FORMAT,
    VN_CONFIG_WRITE_ERROR
} VnConfigResult;

typedef struct VnCapability {
    char name[VN_CONFIG_MAX_CAPABILITY_NAME];
    char command[VN_CONFIG_MAX_CAPABILITY_COMMAND];
} VnCapability;

typedef struct VnConfig {
    char machine[VN_CONFIG_MAX_MACHINE];
    unsigned int node_id;
    char role[VN_CONFIG_MAX_ROLE];
    char ports[VN_CONFIG_MAX_VALUE];
    long baud;
    int discovery_window;
    int discovery_request_count;
    int discovery_request_interval_seconds;
    int discovery_presence_interval_seconds;
    int info_refresh_seconds;
    int info_request_count;
    int info_request_interval_seconds;
    char zmexe[VN_CONFIG_MAX_FILE];
    char zmoptions[VN_CONFIG_MAX_VALUE];
    VnCapability capabilities[VN_CONFIG_MAX_CAPABILITIES];
    int capability_count;
} VnConfig;

typedef struct VnConfigStatus {
    VnConfigResult result;
    int line;
    char key[VN_CONFIG_MAX_CAPABILITY_NAME];
    char message[VN_CONFIG_MAX_VALUE];
} VnConfigStatus;

void vn_config_init_defaults(VnConfig *config);
void vn_config_status_clear(VnConfigStatus *status);
VnConfigResult vn_config_load(const char *filename,
                              VnConfig *config,
                              VnConfigStatus *status);
VnConfigResult vn_config_save(const char *filename,
                              const VnConfig *config,
                              VnConfigStatus *status);
VnConfigResult vn_config_set_machine(VnConfig *config,
                                     const char *value,
                                     VnConfigStatus *status);
VnConfigResult vn_config_set_role(VnConfig *config,
                                  const char *value,
                                  VnConfigStatus *status);
VnConfigResult vn_config_set_ports(VnConfig *config,
                                   const char *value,
                                   VnConfigStatus *status);
VnConfigResult vn_config_set_baud_text(VnConfig *config,
                                       const char *value,
                                       VnConfigStatus *status);
VnConfigResult vn_config_validate(const VnConfig *config,
                                  VnConfigStatus *status);
const char *vn_config_result_text(VnConfigResult result);
int vn_config_needs_setup(const VnConfig *config);

#endif
