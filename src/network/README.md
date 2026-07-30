# Network Protocol Layer

This directory is reserved for future VintaNet packet framing and communication.

Packet/TLV behavior must follow `docs/PROTOCOL_BASELINE.md` and the DOS
VintaNet sources under `/Users/kevincarr/projects/MSDOS/C/VINTANET`.

The network layer will eventually sit above raw serial transport and own packet framing, parsing, addressing, discovery state, INFO messages, and routed command behavior through separate modules.

Not implemented in Phase 1:

- Packet framing
- TLV handling
- Discovery announcements
- Known-machine state
- INFO request/response
- Routed command handling
