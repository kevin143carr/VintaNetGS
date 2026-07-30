# Network Protocol Layer

This directory owns VintaNet packet framing and communication above the raw
serial transport.

The current implementation starts with a narrow packet transport bridge:
`vn_packet_transport` builds packets with the existing protocol library, queues
them through `vn_serial_write`, accumulates serial RX bytes, and extracts
complete packets with `vn_extract_packet`.

Still not implemented:

- Discovery announcements
- Known-machine state
- INFO request/response
- Routed command handling
