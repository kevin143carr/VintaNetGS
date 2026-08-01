#include <misctool.h>
#include <stdio.h>
#include <string.h>

#include "include/vn_network.h"
#include "include/vn_info.h"
#include "include/vn_log.h"
#include "include/vn_serial.h"
#include "include/vn_tlv.h"

#define VN_NETWORK_TICKS_PER_SECOND 60UL
#define VN_NETWORK_DISCOVERY_SLOT_COUNT 10UL
#define VN_NETWORK_DISCOVERY_SLOT_TICKS VN_NETWORK_TICKS_PER_SECOND
#define VN_NETWORK_WARM_CYCLES 3
#define VN_NETWORK_WARM_INTERVAL_SECONDS 2UL
#define VN_NETWORK_INFO_REVISION 1UL
#define VN_NETWORK_FW_COMMAND_BYTE 0x09U
#define VN_NETWORK_SEQ_TRIES 256U
#define VN_NETWORK_NO_SLOT_SEQUENCE 0xFFFFFFFFUL
#define VN_NETWORK_RECENT_PACKET_TICKS \
    (3UL * VN_NETWORK_TICKS_PER_SECOND)

static char vn_network_log_buffer[128];

static void vn_network_mark_dirty(VnNetworkState *state, unsigned int flags)
{
    if (state == 0)
        return;
    state->dirty = 1;
    state->dirty_flags |= flags;
}

static unsigned long vn_network_now_tick(void)
{
    return GetTick();
}

static unsigned long vn_network_seconds_to_ticks(unsigned long seconds)
{
    return seconds * VN_NETWORK_TICKS_PER_SECOND;
}

static void vn_network_copy_string(char *dest,
                                   unsigned int dest_size,
                                   const char *src)
{
    if (dest_size == 0)
        return;
    if (src == 0)
        src = "";
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static char vn_network_upper_char(char value)
{
    if (value >= 'a' && value <= 'z')
        return (char)(value - ('a' - 'A'));
    return value;
}

static int vn_network_string_equal_ci(const char *left, const char *right)
{
    int i;

    if (left == 0)
        left = "";
    if (right == 0)
        right = "";
    for (i = 0; left[i] != '\0' || right[i] != '\0'; i++)
    {
        if (vn_network_upper_char(left[i]) !=
            vn_network_upper_char(right[i]))
            return 0;
    }
    return 1;
}

static int vn_network_local_target_match(const VnConfig *config,
                                         const char *target)
{
    if (config == 0 || target == 0 || target[0] == '\0')
        return 0;
    return vn_network_string_equal_ci(config->machine, target);
}

static void vn_network_set_status(VnNetworkState *state, const char *status)
{
    if (state == 0)
        return;
    if (status == 0)
        status = "";
    if (strcmp(state->status_text, status) == 0)
        return;
    vn_network_copy_string(state->status_text,
                           sizeof(state->status_text),
                           status);
    vn_network_mark_dirty(state, VN_NETWORK_DIRTY_STATUS);
}

static void vn_network_set_packet_status(VnNetworkState *state,
                                         const char *status)
{
    if (state == 0)
        return;
    if (status == 0)
        status = "";
    if (strcmp(state->packet_status, status) == 0)
        return;
    vn_network_copy_string(state->packet_status,
                           sizeof(state->packet_status),
                           status);
    vn_network_mark_dirty(state, VN_NETWORK_DIRTY_STATUS);
}

static unsigned long vn_network_make_session_id(const char *node_name)
{
    unsigned long hash;
    int i;

    hash = 2166136261UL;
    if (node_name == 0)
        node_name = "";
    for (i = 0; node_name[i] != '\0'; i++)
        hash = (hash ^ (unsigned char)node_name[i]) * 16777619UL;
    hash ^= vn_network_now_tick();
    if (hash == 0)
        hash = 1;
    return hash;
}

static unsigned int vn_network_discovery_slot(const char *machine)
{
    unsigned int hash;
    int i;

    hash = 5381U;
    if (machine == 0)
        machine = "";
    for (i = 0; machine[i] != '\0'; i++)
        hash = (unsigned int)(((hash << 5) + hash) ^
                              (unsigned char)machine[i]);
    return hash % (unsigned int)VN_NETWORK_DISCOVERY_SLOT_COUNT;
}

static int vn_network_packet_contains_fw_command(const unsigned char *packet,
                                                 unsigned int packet_length)
{
    unsigned int i;

    if (packet == 0)
        return 0;
    for (i = 0; i < packet_length; i++)
    {
        if (packet[i] == VN_NETWORK_FW_COMMAND_BYTE)
            return 1;
    }
    return 0;
}

static int vn_network_text_contains_fw_command(const char *text)
{
    int i;

    if (text == 0)
        return 0;
    for (i = 0; text[i] != '\0'; i++)
    {
        if ((unsigned char)text[i] == VN_NETWORK_FW_COMMAND_BYTE)
            return 1;
    }
    return 0;
}

static int vn_network_known_node_payload_safe(const VnKnownMachine *machine)
{
    if (machine == 0)
        return 0;
    if ((machine->node_id & 0xFFU) == VN_NETWORK_FW_COMMAND_BYTE ||
        ((machine->node_id >> 8) & 0xFFU) == VN_NETWORK_FW_COMMAND_BYTE)
        return 0;
    if (vn_network_text_contains_fw_command(machine->machine))
        return 0;
    return 1;
}

static int vn_network_recent_seen(VnNetworkState *state,
                                  unsigned char msg_type,
                                  unsigned int node_id,
                                  unsigned long discovery_session_id,
                                  unsigned int discovery_seq)
{
    unsigned int i;
    VnRecentPacket *recent;

    if (node_id == 0 || discovery_session_id == 0)
        return 0;
    for (i = 0; i < VN_NETWORK_RECENT_PACKET_COUNT; i++)
    {
        recent = &state->recent_packets[i];
        if (recent->msg_type == msg_type &&
            recent->node_id == node_id &&
            recent->discovery_session_id == discovery_session_id &&
            recent->discovery_seq == discovery_seq &&
            state->tick - recent->tick < VN_NETWORK_RECENT_PACKET_TICKS)
            return 1;
    }

    recent = &state->recent_packets[state->recent_packet_index];
    recent->msg_type = msg_type;
    recent->node_id = node_id;
    recent->discovery_session_id = discovery_session_id;
    recent->discovery_seq = discovery_seq;
    recent->tick = state->tick;
    state->recent_packet_index++;
    if (state->recent_packet_index >= VN_NETWORK_RECENT_PACKET_COUNT)
        state->recent_packet_index = 0;
    return 0;
}

static void vn_network_clear_recent(VnNetworkState *state)
{
    memset(state->recent_packets, 0, sizeof(state->recent_packets));
    state->recent_packet_index = 0;
}

static int vn_network_clear_pending_info_state(VnNetworkState *state)
{
    int had_pending;

    if (state == 0)
        return 0;
    had_pending = state->info_waiting ||
        state->pending_info_request_id != 0 ||
        state->info_attempt != 0 ||
        state->info_requests_remaining != 0 ||
        state->next_info_tick != 0 ||
        state->info_target[0] != '\0';
    state->info_waiting = 0;
    state->pending_info_request_id = 0;
    state->info_attempt = 0;
    state->info_requests_remaining = 0;
    state->next_info_tick = 0;
    state->info_target[0] = '\0';
    return had_pending;
}

static void vn_network_clear_known(VnNetworkState *state)
{
    memset(state->machines, 0, sizeof(state->machines));
    state->machine_count = 0;
    state->selected_machine = 0;
    vn_network_clear_pending_info_state(state);
    vn_network_mark_dirty(state,
                          VN_NETWORK_DIRTY_MACHINES |
                          VN_NETWORK_DIRTY_DETAILS |
                          VN_NETWORK_DIRTY_STATUS);
}

static int vn_network_find_machine(const VnNetworkState *state,
                                   unsigned int node_id,
                                   const char *machine)
{
    unsigned int i;

    for (i = 0; i < state->machine_count; i++)
    {
        if (state->machines[i].node_id == node_id)
            return (int)i;
        if (machine != 0 &&
            machine[0] != '\0' &&
            vn_network_string_equal_ci(state->machines[i].machine, machine))
            return (int)i;
    }
    return -1;
}

static void vn_network_add_or_update_machine(VnNetworkState *state,
                                             const VnNodeInfo *info,
                                             unsigned int port)
{
    int index;
    VnKnownMachine *machine;
    unsigned long old_session;
    int changed;
    int metadata_present;

    if (info->node_name[0] == '\0')
        return;
    metadata_present = info->role[0] != '\0' ||
        info->info_revision != 0 ||
        info->discovery_session_id != 0;
    index = vn_network_find_machine(state, info->node_id, info->node_name);
    if (index < 0)
    {
        if (state->machine_count >= VN_NETWORK_MAX_KNOWN_MACHINES)
        {
            vn_network_set_status(state, "Known-machine list full.");
            return;
        }
        index = (int)state->machine_count;
        state->machine_count++;
        memset(&state->machines[index], 0, sizeof(state->machines[index]));
        changed = 1;
    }
    else
        changed = 0;

    machine = &state->machines[index];
    old_session = machine->discovery_session_id;
    if (strcmp(machine->machine, info->node_name) != 0 ||
        machine->node_id != info->node_id ||
        machine->port != port ||
        machine->direct != (info->hop_count == 0) ||
        machine->stale)
        changed = 1;
    if (info->discovery_session_id != 0 &&
        machine->discovery_session_id != info->discovery_session_id)
        changed = 1;
    if (info->discovery_seq != 0 &&
        machine->discovery_seq != info->discovery_seq)
        changed = 1;
    if (metadata_present &&
        (strcmp(machine->role, info->role) != 0 ||
         machine->cap_flags != info->cap_flags ||
         machine->info_revision != info->info_revision))
        changed = 1;

    vn_network_copy_string(machine->machine,
                           sizeof(machine->machine),
                           info->node_name);
    machine->node_id = info->node_id;
    if (metadata_present)
    {
        vn_network_copy_string(machine->role,
                               sizeof(machine->role),
                               info->role);
        machine->cap_flags = info->cap_flags;
        machine->info_revision = info->info_revision;
    }
    if (info->discovery_session_id != 0)
        machine->discovery_session_id = info->discovery_session_id;
    if (info->discovery_seq != 0)
        machine->discovery_seq = info->discovery_seq;
    machine->port = port;
    machine->direct = info->hop_count == 0;
    machine->stale = 0;
    machine->last_seen_tick = state->tick;

    if (changed)
    {
        vn_network_mark_dirty(state,
                              VN_NETWORK_DIRTY_MACHINES |
                              VN_NETWORK_DIRTY_DETAILS);
        if (old_session != 0 &&
            old_session != machine->discovery_session_id)
            vn_network_set_status(state, "New discovery session heard.");
        else
            vn_network_set_status(state, "Discovery announce received.");
        vn_network_set_packet_status(state, "RX ANN");
    }
}

static void vn_network_clear_machine_capabilities(VnKnownMachine *machine)
{
    if (machine == 0)
        return;
    memset(machine->capabilities, 0, sizeof(machine->capabilities));
    machine->capability_count = 0;
    machine->selected_capability = 0;
}

static void vn_network_trim_copy(char *dest,
                                 unsigned int dest_size,
                                 const char *start,
                                 unsigned int length)
{
    unsigned int first;
    unsigned int last;
    unsigned int copy_length;

    if (dest_size == 0)
        return;
    dest[0] = '\0';
    if (start == 0 || length == 0)
        return;
    first = 0;
    while (first < length &&
           (start[first] == ' ' || start[first] == '\t'))
        first++;
    last = length;
    while (last > first &&
           (start[last - 1] == ' ' || start[last - 1] == '\t'))
        last--;
    copy_length = last - first;
    if (copy_length >= dest_size)
        copy_length = dest_size - 1;
    if (copy_length > 0)
        memcpy(dest, start + first, copy_length);
    dest[copy_length] = '\0';
}

static void vn_network_store_programs(VnKnownMachine *machine,
                                      const char *programs)
{
    unsigned int i;
    unsigned int start;
    unsigned int length;

    vn_network_clear_machine_capabilities(machine);
    if (machine == 0 || programs == 0)
        return;

    start = 0;
    for (i = 0; ; i++)
    {
        if (programs[i] == ',' || programs[i] == '\0')
        {
            if (machine->capability_count <
                VN_NETWORK_MAX_MACHINE_CAPABILITIES)
            {
                length = i - start;
                vn_network_trim_copy(
                    machine->capabilities[machine->capability_count],
                    sizeof(machine->capabilities[machine->capability_count]),
                    programs + start,
                    length);
                if (machine->capabilities
                        [machine->capability_count][0] != '\0')
                    machine->capability_count++;
            }
            start = i + 1;
        }
        if (programs[i] == '\0')
            break;
    }
}

static void vn_network_update_machine_info(VnNetworkState *state,
                                           const VnNodeInfo *info,
                                           const char *programs,
                                           unsigned int port)
{
    int index;
    VnKnownMachine *machine;

    if (state == 0 || info == 0 || info->node_name[0] == '\0')
        return;
    vn_network_add_or_update_machine(state, info, port);
    index = vn_network_find_machine(state, info->node_id, info->node_name);
    if (index < 0)
        return;

    machine = &state->machines[index];
    vn_network_copy_string(machine->role,
                           sizeof(machine->role),
                           info->role);
    machine->cap_flags = info->cap_flags;
    machine->info_revision = info->info_revision;
    machine->last_info_refresh_tick = state->tick;
    vn_network_store_programs(machine, programs);
    if (machine->selected_capability >= machine->capability_count)
        machine->selected_capability = 0;
    vn_network_mark_dirty(state,
                          VN_NETWORK_DIRTY_MACHINES |
                          VN_NETWORK_DIRTY_DETAILS);
    vn_network_set_status(state, "Info received.");
    vn_network_set_packet_status(state, "RX INFO");
}

static void vn_network_schedule_warm_response(const VnConfig *config,
                                              VnNetworkState *state,
                                              const char *reason)
{
    if (config == 0 || state == 0)
        return;
    if (state->discovery_state != VN_DISCOVERY_COLD)
        return;
    if (state->discovery_session_id == 0)
        state->discovery_session_id =
            vn_network_make_session_id(config->machine);
    state->discovery_state = VN_DISCOVERY_WARM;
    state->warm_cycles_remaining = VN_NETWORK_WARM_CYCLES;
    state->warm_cycles_sent = 0;
    state->next_warm_cycle_tick = state->tick;
    state->discovery_until_tick = state->tick +
        vn_network_seconds_to_ticks(VN_NETWORK_WARM_INTERVAL_SECONDS *
                                    (unsigned long)VN_NETWORK_WARM_CYCLES);
    sprintf(vn_network_log_buffer, "DISCOVERY WARM response %.24s",
            reason == 0 ? "" : reason);
    vn_log_line(vn_network_log_buffer);
    vn_network_set_status(state, "DISCOVERY WARM");
}

static void vn_network_learn_known_nodes(const VnConfig *config,
                                         VnNetworkState *state,
                                         const VnPacket *packet,
                                         unsigned int port)
{
    VnU16 offset;
    VnTlv tlv;
    VnNodeInfo neighbor;
    int result;
    int index;
    int copy_length;

    offset = 0;
    while ((result = vn_tlv_next(packet->payload,
                                 packet->header.payload_len,
                                 &offset,
                                 &tlv)) == 1)
    {
        if (tlv.type != VN_TLV_KNOWN_NODE || tlv.length <= 2)
            continue;
        memset(&neighbor, 0, sizeof(neighbor));
        neighbor.node_id = vn_read_u16_le(tlv.value);
        copy_length = tlv.length - 2;
        if (copy_length > (int)sizeof(neighbor.node_name) - 1)
            copy_length = (int)sizeof(neighbor.node_name) - 1;
        if (copy_length > 0)
            memcpy(neighbor.node_name, tlv.value + 2, copy_length);
        neighbor.node_name[copy_length] = '\0';
        neighbor.hop_count = 1;
        if (vn_network_local_target_match(config, neighbor.node_name))
            continue;
        index = vn_network_find_machine(state,
                                        neighbor.node_id,
                                        neighbor.node_name);
        vn_network_add_or_update_machine(state, &neighbor, port);
        if (index < 0)
        {
            sprintf(vn_network_log_buffer, "KNOWN NODE %.24s",
                    neighbor.node_name);
            vn_log_line(vn_network_log_buffer);
            vn_network_schedule_warm_response(config, state,
                                              neighbor.node_name);
        }
    }
}

static int vn_network_build_announce_packet(const VnConfig *config,
                                            VnNetworkState *state,
                                            VnU16 *packet_length)
{
    unsigned int attempt;
    unsigned int i;
    unsigned int seq;
    VnU16 payload_length;
    VnU16 built_length;
    VnU16 offset;

    if (packet_length == 0)
        return 0;
    for (attempt = 0; attempt < VN_NETWORK_SEQ_TRIES; attempt++)
    {
        seq = (state->discovery_seq + 1U + attempt) & 0xFFFFU;
        if (seq == 0)
            seq = 1U;
        payload_length = 0;
        built_length = 0;
        if (!vn_build_discovery_announce(config,
                                         state->local_info_revision,
                                         state->discovery_session_id,
                                         seq,
                                         state->payload_buffer,
                                         VN_MAX_PAYLOAD,
                                         &payload_length))
            return 0;
        offset = payload_length;
        for (i = 0; i < state->machine_count; i++)
        {
            if (state->machines[i].stale ||
                !vn_network_known_node_payload_safe(&state->machines[i]))
                continue;
            if (!vn_tlv_add_known_node(state->payload_buffer,
                                       VN_MAX_PAYLOAD,
                                       &offset,
                                       state->machines[i].node_id,
                                       state->machines[i].machine))
                break;
        }
        payload_length = offset;
        if (vn_build_packet(state->packet_buffer,
                            VN_MAX_PACKET_SIZE,
                            VN_MSG_DISCOVERY_ANNOUNCE,
                            state->payload_buffer,
                            payload_length,
                            (VnU16)seq,
                            VN_FLAG_NONE,
                            &built_length) != VN_ERR_NONE)
            return 0;
        if (!vn_network_packet_contains_fw_command(state->packet_buffer,
                                                   built_length))
        {
            state->discovery_seq = seq;
            *packet_length = built_length;
            return 1;
        }
    }
    return 0;
}

static int vn_network_send_announce(const VnConfig *config,
                                    VnNetworkState *state)
{
    VnU16 packet_length;
    unsigned int written;

    if (!state->serial_configured)
    {
        vn_network_set_status(state, "PORT 1 REQUIRED");
        return 0;
    }
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        if (!vn_serial_open(1, config->baud))
        {
            state->serial_open = 0;
            vn_network_set_status(state, "DISCOVERY OPEN FAIL");
            sprintf(vn_network_log_buffer,
                    "DISCOVERY TX open_fail status=%d err=%d",
                    (int)vn_serial_status(), vn_serial_stats()->last_error);
            vn_log_line(vn_network_log_buffer);
            return 0;
        }
    }
    state->serial_open = 1;

    packet_length = 0;
    if (!vn_network_build_announce_packet(config, state, &packet_length))
    {
        vn_network_set_status(state, "DISCOVERY BUILD FAIL");
        vn_log_line("DISCOVERY TX build_fail");
        return 0;
    }

    written = vn_serial_diag_write_raw(state->packet_buffer, packet_length);
    if (written == packet_length)
    {
        vn_network_set_packet_status(state, "TX ANN");
    }
    else if (written == 0)
    {
        vn_network_set_status(state, "DISCOVERY TX ERROR");
        vn_network_set_packet_status(state, "TX ERR");
    }
    else
    {
        vn_network_set_status(state, "DISCOVERY TX PARTIAL");
        vn_network_set_packet_status(state, "TX PART");
    }

    sprintf(vn_network_log_buffer,
            "DISCOVERY TX seq=%u len=%u written=%u state=%s err=%d",
            state->discovery_seq, packet_length, written,
            vn_network_discovery_state_text(state->discovery_state),
            vn_serial_stats()->last_error);
    vn_log_line(vn_network_log_buffer);
    return written == packet_length;
}

static int vn_network_send_raw_safe_packet(VnNetworkState *state,
                                           unsigned char msg_type,
                                           VnU16 payload_length,
                                           VnU16 base_sequence,
                                           VnU16 flags,
                                           const char *status_ok,
                                           const char *status_fail,
                                           const char *status_unsafe)
{
    VnU16 packet_length;
    VnU16 sequence;
    unsigned int attempt;
    unsigned int accepted;

    for (attempt = 0; attempt < VN_NETWORK_SEQ_TRIES; attempt++)
    {
        sequence = (VnU16)(base_sequence + attempt);
        if (sequence == 0)
            sequence = 1;
        packet_length = 0;
        if (vn_build_packet(state->packet_buffer,
                            VN_MAX_PACKET_SIZE,
                            msg_type,
                            state->payload_buffer,
                            payload_length,
                            sequence,
                            flags,
                            &packet_length) != VN_ERR_NONE)
        {
            vn_network_set_status(state, status_fail);
            return 0;
        }
        if (vn_network_packet_contains_fw_command(state->packet_buffer,
                                                  packet_length))
            continue;

        accepted = vn_serial_diag_write_raw(state->packet_buffer,
                                            packet_length);
        sprintf(vn_network_log_buffer,
                "RAW SAFE TX type=%u len=%u written=%u seq=%u",
                (unsigned int)msg_type,
                packet_length,
                accepted,
                sequence);
        vn_log_line(vn_network_log_buffer);
        if (accepted == packet_length)
        {
            vn_network_set_status(state, status_ok);
            return 1;
        }
        vn_network_set_status(state, status_fail);
        return 0;
    }

    sprintf(vn_network_log_buffer,
            "RAW SAFE TX unsafe type=%u payload=%u",
            (unsigned int)msg_type,
            payload_length);
    vn_log_line(vn_network_log_buffer);
    vn_network_set_status(state, status_unsafe);
    return 0;
}

static int vn_network_send_info_request(const VnConfig *config,
                                        VnNetworkState *state,
                                        const char *target)
{
    VnU16 payload_length;

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        if (!vn_network_open(config, state, state->serial_configured))
            return 0;
    }

    payload_length = 0;
    if (!vn_build_info_req(config,
                           target,
                           state->pending_info_request_id,
                           VN_DISCOVERY_REQUEST_TTL,
                           state->payload_buffer,
                           VN_MAX_PAYLOAD,
                           &payload_length))
    {
        vn_network_set_status(state, "INFO REQ BUILD FAIL");
        vn_log_line("INFO REQ build_fail");
        return 0;
    }
    sprintf(vn_network_log_buffer,
            "INFO REQ TX target=%.24s id=%u attempt=%u",
            target,
            state->pending_info_request_id,
            state->info_attempt + 1U);
    vn_log_line(vn_network_log_buffer);
    vn_network_set_packet_status(state, "TX INFO");
    return vn_network_send_raw_safe_packet(
        state,
        VN_MSG_INFO_REQ,
        payload_length,
        (VnU16)state->pending_info_request_id,
        VN_FLAG_NONE,
        "Info requested.",
        "INFO REQ TX FAIL",
        "INFO REQ UNSAFE");
}

static int vn_network_send_info_response(const VnConfig *config,
                                         VnNetworkState *state,
                                         const VnNodeInfo *request)
{
    VnU16 payload_length;

    if (request == 0)
        return 0;
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
    {
        if (!vn_network_open(config, state, state->serial_configured))
            return 0;
    }

    payload_length = 0;
    if (!vn_build_info_resp_basic(config,
                                  request->node_name,
                                  state->local_info_revision,
                                  request->request_id,
                                  state->payload_buffer,
                                  VN_MAX_PAYLOAD,
                                  &payload_length))
    {
        vn_network_set_status(state, "INFO RESP BUILD FAIL");
        vn_log_line("INFO RESP build_fail");
        return 0;
    }
    sprintf(vn_network_log_buffer,
            "INFO RESP TX target=%.24s id=%u",
            request->node_name,
            request->request_id);
    vn_log_line(vn_network_log_buffer);
    vn_network_set_packet_status(state, "TX INFO");
    return vn_network_send_raw_safe_packet(
        state,
        VN_MSG_INFO_RESP,
        payload_length,
        (VnU16)request->request_id,
        VN_FLAG_IS_RESPONSE,
        "Info response sent.",
        "INFO RESP TX FAIL",
        "INFO RESP UNSAFE");
}

static int vn_network_hot_slot_due(const VnConfig *config,
                                   VnNetworkState *state)
{
    unsigned long slot_sequence;
    unsigned long current_slot;
    unsigned int local_slot;
    if (state->discovery_state != VN_DISCOVERY_HOT)
        return 0;
    if (state->tick > state->discovery_until_tick)
        return 0;
    local_slot = vn_network_discovery_slot(config->machine);
    slot_sequence = state->tick / VN_NETWORK_DISCOVERY_SLOT_TICKS;
    current_slot = slot_sequence % VN_NETWORK_DISCOVERY_SLOT_COUNT;
    if (current_slot != local_slot)
        return 0;
    if (state->last_discovery_slot_sequence == slot_sequence)
        return 0;
    state->last_discovery_slot_sequence = slot_sequence;
    return 1;
}

static void vn_network_finish_hot(VnNetworkState *state)
{
    state->discovery_state = VN_DISCOVERY_WARM;
    state->warm_cycles_remaining = VN_NETWORK_WARM_CYCLES;
    state->warm_cycles_sent = 0;
    state->next_warm_cycle_tick = state->tick;
    state->discovery_until_tick = state->tick +
        vn_network_seconds_to_ticks(VN_NETWORK_WARM_INTERVAL_SECONDS *
                                    (unsigned long)VN_NETWORK_WARM_CYCLES);
    vn_network_set_status(state, "DISCOVERY HOT->WARM");
    vn_log_line("DISCOVERY HOT->WARM");
}

static void vn_network_finish_warm(VnNetworkState *state)
{
    state->discovery_state = VN_DISCOVERY_COLD;
    state->warm_cycles_remaining = 0;
    state->warm_cycles_sent = 0;
    state->next_warm_cycle_tick = 0;
    state->last_discovery_slot_sequence = VN_NETWORK_NO_SLOT_SEQUENCE;
    vn_network_set_status(state, "DISCOVERY COLD");
    vn_log_line("DISCOVERY WARM->COLD");
}

static int vn_network_warm_due(VnNetworkState *state)
{
    if (state->discovery_state == VN_DISCOVERY_HOT)
    {
        if (state->tick <= state->discovery_until_tick)
            return 0;
        vn_network_finish_hot(state);
    }
    if (state->discovery_state != VN_DISCOVERY_WARM)
        return 0;
    if (state->warm_cycles_remaining <= 0)
    {
        vn_network_finish_warm(state);
        return 0;
    }
    if (state->tick < state->next_warm_cycle_tick)
        return 0;
    state->warm_cycles_sent++;
    state->warm_cycles_remaining--;
    state->next_warm_cycle_tick = state->tick +
        vn_network_seconds_to_ticks(VN_NETWORK_WARM_INTERVAL_SECONDS);
    if (state->warm_cycles_remaining <= 0)
        vn_network_finish_warm(state);
    return 1;
}

static void vn_network_handle_packet(const VnConfig *config,
                                     VnNetworkState *state,
                                     const VnPacket *packet)
{
    VnNodeInfo info;
    int index;
    unsigned long old_session;

    if (packet->header.msg_type == VN_MSG_DISCOVERY_ANNOUNCE)
    {
        if (!vn_parse_node_info(packet->payload,
                                packet->header.payload_len,
                                &info))
            return;
        if (info.node_id == vn_config_node_id(config) ||
            vn_network_string_equal_ci(info.node_name, config->machine))
            return;
        if (vn_network_recent_seen(state,
                                   packet->header.msg_type,
                                   info.node_id,
                                   info.discovery_session_id,
                                   info.discovery_seq))
        {
            vn_log_line("DROP ANNOUNCE duplicate");
            return;
        }
        index = vn_network_find_machine(state, info.node_id,
                                        info.node_name);
        old_session = index >= 0 ?
            state->machines[index].discovery_session_id : 0;
        sprintf(vn_network_log_buffer,
                "ANN RX %.24s sid=%08lX seq=%u",
                info.node_name,
                info.discovery_session_id,
                info.discovery_seq);
        vn_log_line(vn_network_log_buffer);
        vn_network_add_or_update_machine(state, &info, 1U);
        vn_network_learn_known_nodes(config, state, packet, 1U);
        if (index < 0 ||
            (old_session != 0 &&
             info.discovery_session_id != 0 &&
             old_session != info.discovery_session_id))
            vn_network_schedule_warm_response(config, state,
                                              info.node_name);
        return;
    }

    if (packet->header.msg_type == VN_MSG_INFO_REQ)
    {
        if (!vn_parse_node_info(packet->payload,
                                packet->header.payload_len,
                                &info))
            return;
        if (info.node_id == vn_config_node_id(config) ||
            vn_network_string_equal_ci(info.node_name, config->machine))
            return;
        vn_network_add_or_update_machine(state, &info, 1U);
        sprintf(vn_network_log_buffer,
                "INFO REQ RX %.24s->%.24s id=%u",
                info.node_name,
                info.target_name,
                info.request_id);
        vn_log_line(vn_network_log_buffer);
        if (vn_network_local_target_match(config, info.target_name))
            vn_network_send_info_response(config, state, &info);
        else
        {
            vn_network_set_status(state, "INFO REQ not local.");
            vn_network_set_packet_status(state, "RX INFO");
        }
        return;
    }

    if (packet->header.msg_type == VN_MSG_INFO_RESP)
    {
        state->info_programs[0] = '\0';
        if (!vn_parse_info_resp(packet->payload,
                                packet->header.payload_len,
                                &info,
                                state->info_programs,
                                sizeof(state->info_programs)))
            return;
        if (info.node_id == vn_config_node_id(config) ||
            vn_network_string_equal_ci(info.node_name, config->machine))
            return;
        sprintf(vn_network_log_buffer,
                "INFO RESP RX %.24s->%.24s id=%u",
                info.node_name,
                info.target_name,
                info.request_id);
        vn_log_line(vn_network_log_buffer);
        if (!vn_network_local_target_match(config, info.target_name))
        {
            vn_network_set_status(state, "INFO RESP not local.");
            vn_network_set_packet_status(state, "RX INFO");
            return;
        }
        vn_network_update_machine_info(state, &info,
                                       state->info_programs, 1U);
        if (state->info_waiting &&
            state->pending_info_request_id == info.request_id &&
            vn_network_string_equal_ci(state->info_target,
                                       info.node_name))
        {
            state->info_waiting = 0;
            state->info_requests_remaining = 0;
            state->info_attempt = 0;
            state->pending_info_request_id = 0;
            state->info_target[0] = '\0';
            state->next_info_tick = 0;
        }
        return;
    }
}

static void vn_network_poll_receive(const VnConfig *config,
                                    VnNetworkState *state)
{
    unsigned char value;
    int read_result;
    int extract_result;

    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN)
        return;
    vn_serial_poll();
    while (state->receive_length < (int)VN_NETWORK_RECEIVE_SIZE)
    {
        read_result = vn_serial_read_byte(&value);
        if (read_result <= 0)
            break;
        state->receive_buffer[state->receive_length++] = value;
    }
    if (state->receive_length >= (int)VN_NETWORK_RECEIVE_SIZE)
    {
        state->receive_length = 0;
        vn_network_set_status(state, "RX BUFFER CLEARED");
        return;
    }

    do
    {
        extract_result = vn_extract_packet(state->receive_buffer,
                                           &state->receive_length,
                                           &state->packet);
        if (extract_result == 1)
            vn_network_handle_packet(config, state, &state->packet);
    } while (extract_result == 1);
}

static unsigned long vn_network_info_interval_ticks(const VnConfig *config)
{
    int seconds;

    seconds = config->info_request_interval_seconds;
    if (seconds <= 0)
        seconds = VN_CONFIG_DEFAULT_INFO_REQUEST_INTERVAL_SECONDS;
    return vn_network_seconds_to_ticks((unsigned long)seconds);
}

static int vn_network_info_request_count(const VnConfig *config)
{
    if (config->info_request_count <= 0)
        return VN_CONFIG_DEFAULT_INFO_REQUEST_COUNT;
    return config->info_request_count;
}

static void vn_network_poll_info(const VnConfig *config,
                                 VnNetworkState *state)
{
    if (!state->info_waiting)
        return;
    if (state->info_requests_remaining <= 0)
    {
        state->info_waiting = 0;
        state->pending_info_request_id = 0;
        state->info_attempt = 0;
        state->next_info_tick = 0;
        vn_network_set_status(state, "Info request timed out.");
        vn_network_set_packet_status(state, "INFO WAIT");
        return;
    }
    if (state->machine_count == 0 ||
        state->selected_machine >= state->machine_count ||
        !vn_network_string_equal_ci(
            state->info_target,
            state->machines[state->selected_machine].machine))
    {
        vn_network_cancel_pending_info(state, "selection");
        return;
    }
    if (state->tick < state->next_info_tick)
        return;

    vn_network_send_info_request(config, state, state->info_target);
    state->info_attempt++;
    state->info_requests_remaining--;
    if (state->info_requests_remaining > 0)
        state->next_info_tick = state->tick +
            vn_network_info_interval_ticks(config);
    else
        state->next_info_tick = state->tick +
            vn_network_info_interval_ticks(config);
}

const char *vn_network_discovery_state_text(VnDiscoveryState state)
{
    if (state == VN_DISCOVERY_HOT)
        return "HOT";
    if (state == VN_DISCOVERY_WARM)
        return "WARM";
    return "COLD";
}

void vn_network_init(VnNetworkState *state)
{
    if (state == 0)
        return;
    memset(state, 0, sizeof(VnNetworkState));
    state->discovery_state = VN_DISCOVERY_COLD;
    state->last_discovery_slot_sequence = VN_NETWORK_NO_SLOT_SEQUENCE;
    state->local_info_revision = VN_NETWORK_INFO_REVISION;
    state->next_info_request_id = 1U;
    vn_network_copy_string(state->status_text,
                           sizeof(state->status_text),
                           "Setup needed.");
    vn_network_copy_string(state->packet_status,
                           sizeof(state->packet_status),
                           "PKT READY");
    vn_network_mark_dirty(state, VN_NETWORK_DIRTY_ALL);
}

int vn_network_open(const VnConfig *config,
                    VnNetworkState *state,
                    int serial_configured)
{
    if (state == 0)
        return 0;
    state->serial_configured = serial_configured;
    state->tick = vn_network_now_tick();
    if (config == 0 || vn_config_needs_setup(config))
    {
        state->serial_open = 0;
        vn_network_set_status(state, "Setup needed.");
        return 0;
    }
    if (!serial_configured)
    {
        state->serial_open = 0;
        vn_network_set_status(state, "PORT 1 REQUIRED");
        return 0;
    }
    if (vn_serial_status() != VN_SERIAL_STATUS_OPEN &&
        !vn_serial_open(1, config->baud))
    {
        state->serial_open = 0;
        vn_network_set_status(state, "SERIAL OPEN FAIL");
        return 0;
    }
    state->serial_open = 1;
    vn_network_set_status(state, "Ready.");
    return 1;
}

void vn_network_close(VnNetworkState *state)
{
    if (state != 0)
        state->serial_open = 0;
    vn_serial_close();
}

void vn_network_cancel_pending_info(VnNetworkState *state,
                                    const char *reason)
{
    if (!vn_network_clear_pending_info_state(state))
        return;
    sprintf(vn_network_log_buffer, "INFO CANCEL %.32s",
            reason == 0 ? "" : reason);
    vn_log_line(vn_network_log_buffer);
    vn_network_set_status(state, "Info canceled.");
    vn_network_set_packet_status(state, "INFO IDLE");
}

void vn_network_wake_discovery(const VnConfig *config,
                               VnNetworkState *state)
{
    int seconds;

    if (state == 0 || config == 0 || vn_config_needs_setup(config))
        return;
    state->tick = vn_network_now_tick();
    state->discovery_session_id =
        vn_network_make_session_id(config->machine);
    state->discovery_seq = 0;
    seconds = config->discovery_window;
    if (seconds <= 0)
        seconds = 30;
    state->discovery_until_tick = state->tick +
        vn_network_seconds_to_ticks((unsigned long)seconds);
    state->discovery_state = VN_DISCOVERY_HOT;
    state->warm_cycles_remaining = 0;
    state->warm_cycles_sent = 0;
    state->next_warm_cycle_tick = 0;
    state->last_discovery_slot_sequence = VN_NETWORK_NO_SLOT_SEQUENCE;
    state->receive_length = 0;
    vn_network_clear_pending_info_state(state);
    vn_network_clear_recent(state);
    vn_network_set_status(state, "DISCOVERY HOT");
    vn_log_line("DISCOVERY HOT start");
}

void vn_network_start_manual_discovery(const VnConfig *config,
                                       VnNetworkState *state)
{
    if (state == 0 || config == 0 || vn_config_needs_setup(config))
    {
        vn_network_set_status(state, "CONFIG REQUIRED");
        return;
    }
    state->tick = vn_network_now_tick();
    state->discovery_session_id =
        vn_network_make_session_id(config->machine);
    state->discovery_seq = 0;
    state->discovery_state = VN_DISCOVERY_WARM;
    state->warm_cycles_remaining = VN_NETWORK_WARM_CYCLES;
    state->warm_cycles_sent = 0;
    state->next_warm_cycle_tick = state->tick;
    state->last_discovery_slot_sequence = VN_NETWORK_NO_SLOT_SEQUENCE;
    state->discovery_until_tick = state->tick +
        vn_network_seconds_to_ticks(VN_NETWORK_WARM_INTERVAL_SECONDS *
                                    (unsigned long)VN_NETWORK_WARM_CYCLES);
    state->receive_length = 0;
    vn_network_clear_pending_info_state(state);
    vn_network_clear_known(state);
    vn_network_clear_recent(state);
    vn_network_set_status(state, "DISCOVERY WARM");
    vn_log_line("DISCOVERY WARM manual");
}

void vn_network_poll(const VnConfig *config, VnNetworkState *state)
{
    if (state == 0 || config == 0)
        return;
    state->tick = vn_network_now_tick();
    if (state->serial_configured && vn_serial_status() != VN_SERIAL_STATUS_OPEN)
        state->serial_open = 0;
    vn_network_poll_receive(config, state);
    if (vn_network_hot_slot_due(config, state))
        vn_network_send_announce(config, state);
    if (vn_network_warm_due(state))
        vn_network_send_announce(config, state);
    vn_network_poll_info(config, state);
}

void vn_network_select_previous_machine(VnNetworkState *state)
{
    if (state == 0)
        return;
    vn_network_cancel_pending_info(state, "navigation");
    if (state->machine_count == 0)
        return;
    if (state->selected_machine == 0)
        state->selected_machine = state->machine_count - 1U;
    else
        state->selected_machine--;
    vn_network_mark_dirty(state,
                          VN_NETWORK_DIRTY_MACHINES |
                          VN_NETWORK_DIRTY_DETAILS);
}

void vn_network_select_next_machine(VnNetworkState *state)
{
    if (state == 0)
        return;
    vn_network_cancel_pending_info(state, "navigation");
    if (state->machine_count == 0)
        return;
    state->selected_machine++;
    if (state->selected_machine >= state->machine_count)
        state->selected_machine = 0;
    vn_network_mark_dirty(state,
                          VN_NETWORK_DIRTY_MACHINES |
                          VN_NETWORK_DIRTY_DETAILS);
}

void vn_network_select_previous_capability(VnNetworkState *state)
{
    VnKnownMachine *machine;

    if (state == 0 || state->machine_count == 0 ||
        state->selected_machine >= state->machine_count)
        return;
    machine = &state->machines[state->selected_machine];
    if (machine->capability_count == 0)
        return;
    if (machine->selected_capability == 0)
        machine->selected_capability = machine->capability_count - 1U;
    else
        machine->selected_capability--;
    vn_network_mark_dirty(state, VN_NETWORK_DIRTY_DETAILS);
}

void vn_network_select_next_capability(VnNetworkState *state)
{
    VnKnownMachine *machine;

    if (state == 0 || state->machine_count == 0 ||
        state->selected_machine >= state->machine_count)
        return;
    machine = &state->machines[state->selected_machine];
    if (machine->capability_count == 0)
        return;
    machine->selected_capability++;
    if (machine->selected_capability >= machine->capability_count)
        machine->selected_capability = 0;
    vn_network_mark_dirty(state, VN_NETWORK_DIRTY_DETAILS);
}

int vn_network_request_selected_info(const VnConfig *config,
                                     VnNetworkState *state)
{
    VnKnownMachine *machine;

    if (state == 0 || config == 0 || vn_config_needs_setup(config))
    {
        vn_network_set_status(state, "CONFIG REQUIRED");
        return 0;
    }
    if (state->machine_count == 0 ||
        state->selected_machine >= state->machine_count)
    {
        vn_network_set_status(state, "No machine selected.");
        return 0;
    }
    machine = &state->machines[state->selected_machine];
    if (machine->capability_count > 0)
    {
        vn_network_set_status(state, "Launch not implemented.");
        return 0;
    }
    if (state->info_waiting &&
        vn_network_string_equal_ci(state->info_target, machine->machine))
    {
        vn_network_set_status(state, "Info request pending.");
        return 1;
    }

    state->pending_info_request_id = state->next_info_request_id++;
    if (state->next_info_request_id == 0)
        state->next_info_request_id = 1U;
    state->info_attempt = 0;
    state->info_requests_remaining =
        vn_network_info_request_count(config);
    state->info_waiting = 1;
    state->next_info_tick = state->tick;
    vn_network_copy_string(state->info_target,
                           sizeof(state->info_target),
                           machine->machine);
    machine->last_info_request_tick = state->tick;
    vn_network_set_status(state, "Requesting info.");
    vn_network_poll_info(config, state);
    return 1;
}

void vn_network_fill_dashboard(const VnNetworkState *state,
                               VnUiDashboardDisplay *display)
{
    unsigned int i;
    const VnKnownMachine *machine;
    VnUiDashboardMachine *row;

    if (state == 0 || display == 0)
        return;
    display->packet_status = state->packet_status;
    display->status_text = state->status_text;
    display->machine_count = state->machine_count;
    display->selected_machine = state->selected_machine;
    display->selected_capability = 0;
    for (i = 0; i < VN_UI_DASHBOARD_MACHINE_ROWS; i++)
    {
        row = &display->machines[i];
        memset(row, 0, sizeof(*row));
        if (i >= state->machine_count)
            continue;
        machine = &state->machines[i];
        vn_network_copy_string(row->machine,
                               sizeof(row->machine),
                               machine->machine);
        vn_network_copy_string(row->role,
                               sizeof(row->role),
                               machine->role);
        sprintf(row->port, "P%u", machine->port);
        vn_network_copy_string(row->route,
                               sizeof(row->route),
                               machine->direct ? "DIRECT" : "ROUTE");
        row->capability_count = machine->capability_count;
        if (i == state->selected_machine)
            display->selected_capability = machine->selected_capability;
        vn_network_copy_string(row->selected_capability,
                               sizeof(row->selected_capability),
                               machine->capability_count == 0 ? "--" :
                               machine->capabilities
                                   [machine->selected_capability]);
    }
}
