#ifndef VN_SCC_H
#define VN_SCC_H

typedef struct VnSccConfig {
    int slot;
    long baud;
} VnSccConfig;

int vn_scc_open(const VnSccConfig *config);
void vn_scc_close(void);
int vn_scc_tx_ready(void);
int vn_scc_write_byte(unsigned char value);
int vn_scc_rx_ready(void);
int vn_scc_read_byte(unsigned char *value);
const char *vn_scc_status_text(void);

#endif
