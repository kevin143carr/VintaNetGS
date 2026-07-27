#include "include/vn_scc.h"

static int vn_scc_opened;

int vn_scc_open(const VnSccConfig *config)
{
    (void)config;
    vn_scc_opened = 0;
    return 0;
}

void vn_scc_close(void)
{
    vn_scc_opened = 0;
}

int vn_scc_tx_ready(void)
{
    (void)vn_scc_opened;
    return -1;
}

int vn_scc_write_byte(unsigned char value)
{
    (void)value;
    return 0;
}

int vn_scc_rx_ready(void)
{
    (void)vn_scc_opened;
    return -1;
}

int vn_scc_read_byte(unsigned char *value)
{
    (void)value;
    return -1;
}

const char *vn_scc_status_text(void)
{
    return "SCC LANE UNIMPLEMENTED";
}
