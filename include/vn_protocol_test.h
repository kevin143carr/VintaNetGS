#ifndef VN_PROTOCOL_TEST_H
#define VN_PROTOCOL_TEST_H

#define VN_PROTOCOL_TEST_NAME_SIZE 48

typedef struct VnProtocolTestResult {
    int total;
    int passed;
    int failed;
    char first_failed[VN_PROTOCOL_TEST_NAME_SIZE];
} VnProtocolTestResult;

void vn_protocol_test_run(VnProtocolTestResult *result);

#endif
