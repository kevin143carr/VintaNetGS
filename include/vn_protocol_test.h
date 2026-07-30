#ifndef VN_PROTOCOL_TEST_H
#define VN_PROTOCOL_TEST_H

#include "include/vn_protocol.h"

#define VN_PROTOCOL_TEST_NAME_SIZE 48

typedef void (*VnProtocolTestLineFn)(const char *line, void *context);

typedef struct VnProtocolTestResult {
    int total;
    int passed;
    int failed;
    char first_failed[VN_PROTOCOL_TEST_NAME_SIZE];
} VnProtocolTestResult;

void vn_protocol_test_run(VnProtocolTestResult *result);
void vn_protocol_test_run_emit(VnProtocolTestResult *result,
                               VnProtocolTestLineFn line_fn,
                               void *context);
int vn_protocol_test_build_discovery_announce(VnU8 *packet,
                                              VnU16 packet_capacity,
                                              VnU16 *packet_length);

#endif
