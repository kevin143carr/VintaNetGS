# Tests

This directory is reserved for future VintaNetGS tests and validation notes.

Phase 1 validation is performed through the shared Apple IIgs workflow:

```sh
./build.sh build
./build.sh verify
```

Interactive rendering, keyboard behavior, and serial probing must be verified in GSplus or on real hardware. A successful `iix` launch or build is not proof that direct hardware text rendering or serial firmware behavior works in the emulator.

Configuration parser fixtures live in `tests/config/`. They document accepted and rejected file shapes for the current fixed-size parser. There is not yet an automated fixture runner inside ORCA/C.

Cross-platform VintaNet protocol testing is owned by:

```text
/Users/kevincarr/projects/VintaNetTestDriver
```

VintaNetGS supports that driver by running the protocol test suite at startup
when `VINTANETGS.TEST` contains `PROTOCOL`.  That test mode writes stable
`TEST`, `VECTOR`, `SUMMARY`, and `VNTEST RESULT` lines to `VINTANETGS.LOG` and
then exits.  The marker file is separate from `VINTANETGS.CFG` so normal config
semantics do not change.

Serial TLV proof must use GSplus or real hardware.  In the serial diagnostics
screen, `X` sends the direct-test `DISCOVERY_ANNOUNCE` reference packet and
`K` receives, byte-compares, extracts, and parses that same packet.  The
VintaNetTestDriver `serial-capture` and `serial-send` commands provide the
host-side GSplus TCP helpers for those manual diagnostics.
