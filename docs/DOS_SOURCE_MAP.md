# DOS Source Map

The DOS VintaNet repository is a behavioral and architectural reference only. Do not copy DOS code into VintaNetGS. Each DOS responsibility must be documented before changing protocol behavior in the Apple IIgs implementation.

## Proposed Mapping

```text
VINTANET.CPP      -> vn_main.c and vn_app.c
VNUI.CPP/.H       -> src/ui/vn_ui.c and include/vn_ui.h
CONFIG.CPP        -> src/config/vn_config.c and include/vn_config.h
NETWORK.CPP/.H    -> future serial and network layers
VNPROTO.CPP/.H    -> future packet framing module
VNTLV.CPP/.H      -> future TLV module
VNDISCOV.CPP/.H   -> future discovery module
VNINFO.CPP/.H     -> future INFO module
VNLAUNCH.CPP/.H   -> future command module
VNACT.CPP/.H      -> future application action layer
VNKEY components  -> future remote-control investigation
```

## Notes

`NETWORK.CPP` mixes several responsibilities that must be separated in VintaNetGS:

- Raw serial transport
- Packet send and receive
- Addressing behavior
- Discovery-related state
- Higher-level command handling

The Apple IIgs project should split those responsibilities into serial, packet, discovery, INFO, and command modules as the migration phases are accepted.

DOS-specific constructs must not be carried forward directly:

- UART register code
- Borland-specific APIs
- `far` pointers
- Interrupt handlers
- TSR behavior
- `conio` APIs
- BIOS calls

The VintaNet protocol should remain stable unless the DOS behavior is first documented and a deliberate VintaNetGS change is accepted.

## Configuration Notes

The DOS loader starts with defaults, reads `VINTANET.CFG`, accepts flat `KEY = VALUE` lines, recognizes `[CAPABILITIES]`, and stores fixed-size values in `VintaConfig`.

VintaNetGS uses the same broad responsibility split but with Apple IIgs-specific names:

- Default runtime filename: `VINTANETGS.CFG`.
- Runtime location: current working directory.
- Blank `MACHINE` and blank `PORTS` remain valid setup-needed values.
- All DOS fields are represented and parsed.
- `[CAPABILITIES]` entries are parsed and counted, but not executed.
- Serial, discovery, INFO, launch, and remote-control behavior remain deferred.
