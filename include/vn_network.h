#ifndef VN_NETWORK_H
#define VN_NETWORK_H

#include "include/vn_config.h"
#include "include/vn_discovery.h"
#include "include/vn_protocol.h"
#include "include/vn_ui.h"

#define VN_NETWORK_MAX_KNOWN_MACHINES VN_UI_DASHBOARD_MACHINE_ROWS
#define VN_NETWORK_MAX_MACHINE_CAPABILITIES 8U
#define VN_NETWORK_RECENT_PACKET_COUNT 8U
#define VN_NETWORK_RECEIVE_SIZE (VN_MAX_PACKET_SIZE + 12U)
#define VN_NETWORK_DIRTY_STATUS 0x0001U
#define VN_NETWORK_DIRTY_MACHINES 0x0002U
#define VN_NETWORK_DIRTY_DETAILS 0x0004U
#define VN_NETWORK_DIRTY_LAYOUT 0x0008U
#define VN_NETWORK_DIRTY_ALL 0x000FU

typedef enum VnDiscoveryState {
    VN_DISCOVERY_COLD = 0,
    VN_DISCOVERY_HOT,
    VN_DISCOVERY_WARM
} VnDiscoveryState;

typedef struct VnKnownMachine {
    char machine[VN_CONFIG_MAX_MACHINE];
    unsigned int node_id;
    char role[VN_CONFIG_MAX_ROLE];
    unsigned long cap_flags;
    unsigned long info_revision;
    char capabilities[VN_NETWORK_MAX_MACHINE_CAPABILITIES]
        [VN_CONFIG_MAX_CAPABILITY_NAME];
    unsigned int capability_count;
    unsigned int selected_capability;
    unsigned long last_info_refresh_tick;
    unsigned long last_info_request_tick;
    unsigned long discovery_session_id;
    unsigned int discovery_seq;
    unsigned int port;
    int direct;
    int stale;
    unsigned long last_seen_tick;
} VnKnownMachine;

typedef struct VnRecentPacket {
    unsigned char msg_type;
    unsigned int node_id;
    unsigned long discovery_session_id;
    unsigned int discovery_seq;
    unsigned long tick;
} VnRecentPacket;

typedef struct VnNetworkState {
    VnKnownMachine machines[VN_NETWORK_MAX_KNOWN_MACHINES];
    unsigned int machine_count;
    unsigned int selected_machine;
    VnRecentPacket recent_packets[VN_NETWORK_RECENT_PACKET_COUNT];
    unsigned int recent_packet_index;
    unsigned char receive_buffer[VN_NETWORK_RECEIVE_SIZE];
    int receive_length;
    unsigned char packet_buffer[VN_MAX_PACKET_SIZE];
    unsigned char payload_buffer[VN_MAX_PAYLOAD];
    char info_programs[VN_CONFIG_MAX_VALUE];
    VnPacket packet;
    unsigned long tick;
    VnDiscoveryState discovery_state;
    unsigned long discovery_until_tick;
    unsigned long next_warm_cycle_tick;
    unsigned long last_discovery_slot_sequence;
    unsigned long discovery_session_id;
    unsigned int discovery_seq;
    unsigned int next_info_request_id;
    unsigned int pending_info_request_id;
    unsigned int info_attempt;
    int info_requests_remaining;
    int info_waiting;
    char info_target[VN_CONFIG_MAX_MACHINE];
    unsigned long next_info_tick;
    int warm_cycles_remaining;
    int warm_cycles_sent;
    unsigned long local_info_revision;
    int serial_configured;
    int serial_open;
    int dirty;
    unsigned int dirty_flags;
    char status_text[40];
    char packet_status[16];
} VnNetworkState;

void vn_network_init(VnNetworkState *state);
int vn_network_open(const VnConfig *config,
                    VnNetworkState *state,
                    int serial_configured);
void vn_network_close(VnNetworkState *state);
void vn_network_wake_discovery(const VnConfig *config,
                               VnNetworkState *state);
void vn_network_start_manual_discovery(const VnConfig *config,
                                       VnNetworkState *state);
void vn_network_poll(const VnConfig *config, VnNetworkState *state);
void vn_network_cancel_pending_info(VnNetworkState *state,
                                    const char *reason);
void vn_network_select_previous_machine(VnNetworkState *state);
void vn_network_select_next_machine(VnNetworkState *state);
void vn_network_select_previous_capability(VnNetworkState *state);
void vn_network_select_next_capability(VnNetworkState *state);
int vn_network_request_selected_info(const VnConfig *config,
                                     VnNetworkState *state);
void vn_network_fill_dashboard(const VnNetworkState *state,
                               VnUiDashboardDisplay *display);
const char *vn_network_discovery_state_text(VnDiscoveryState state);

#endif
