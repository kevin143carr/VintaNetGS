# Network Protocol Layer

This directory is reserved for future VintaNet packet framing and communication.

Packet/TLV behavior must follow `docs/PROTOCOL_BASELINE.md` and the DOS
VintaNet sources under `/Users/kevincarr/projects/MSDOS/C/VINTANET`.

The implemented offline helpers are:

- DOS-compatible `DISCOVERY_ANNOUNCE` payload construction/parsing, based on
  `VNDISCOV.H` and `VNDISCOV.CPP`.
- DOS-compatible explicit `INFO_REQ` and `INFO_RESP` payload
  construction/parsing, based on `VNINFO.H` and `VNINFO.CPP`.

The network layer will eventually sit above raw serial transport and own packet framing, parsing, addressing, discovery state, INFO messages, and routed command behavior through separate modules.

Not implemented in Phase 1:

- Live packet transmission over serial from the network layer
- Live discovery announcements
- Known-machine state
- Automatic INFO polling
- Routed command handling
