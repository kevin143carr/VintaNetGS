# Tests

This directory is reserved for future VintaNetGS tests and validation notes.

Phase 1 validation is performed through the shared Apple IIgs workflow:

```sh
./build.sh build
./build.sh verify
```

Interactive rendering and keyboard behavior must eventually be verified in Clemens. A successful `iix` launch or build is not proof that direct hardware text rendering behaves correctly in the emulator.

Configuration parser fixtures live in `tests/config/`. They document accepted and rejected file shapes for the current fixed-size parser. There is not yet an automated fixture runner inside ORCA/C.
