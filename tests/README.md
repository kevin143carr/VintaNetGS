# Tests

This directory is reserved for future VintaNetGS tests and validation notes.

Phase 1 validation is performed through the shared Apple IIgs workflow:

```sh
./build.sh build
./build.sh verify
```

Interactive rendering, keyboard behavior, and serial probing must be verified in GSplus or on real hardware. A successful `iix` launch or build is not proof that direct hardware text rendering or serial firmware behavior works in the emulator.

Configuration parser fixtures live in `tests/config/`. They document accepted and rejected file shapes for the current fixed-size parser. There is not yet an automated fixture runner inside ORCA/C.
