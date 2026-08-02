# VintaNetGS

VintaNetGS is an Apple IIgs ORCA/C porting project for the VintaNet serial-network concept. This repository is the Apple IIgs project shell only; the DOS VintaNet repository is a behavioral and architectural reference, not source to copy into this build.

## Current Status

The project currently contains only Phase 1: development structure and a minimal TEXTUIGS-based UI shell. It initializes 40-column text mode, draws a deterministic status screen, waits for Escape or `Q`, restores the display state, and exits.

Configuration loading is now present. Raw serial transport is under manual
diagnostic validation only. Packet communications, discovery, INFO handling,
routed commands, remote launch, and remote-control behavior are deliberately
absent.

## Dependencies

This project consumes existing sibling repositories:

```text
/Users/kevincarr/projects/AppleIIGS/C/TEXTUIGS
/Users/kevincarr/projects/Devel_Ops
```

VintaNetGS does not vendor or copy TEXTUIGS. The ORCA/C workflow wrapper includes the TEXTUIGS implementation from `../TEXTUIGS/src/` and then includes VintaNetGS implementation files from `src/`.

The local `include/textui*.h` and `src/textui_internal.h` files are forwarding adapters for ORCA/C include resolution. They include the real TEXTUIGS headers from `../TEXTUIGS` and do not contain copied TEXTUIGS implementation.

## Build

Build the local S16 executable:

```sh
./build.sh
./build.sh build
```

Other supported wrapper stages include:

```sh
./build.sh run-iix
./build.sh import
./build.sh run-gsos
./build.sh verify
```

The default stage is `build`. Build, import, verify, and `iix` stages use the
shared Devel_Ops workflow. GS/OS emulator stages are handled by this project's
wrapper so VintaNetGS can launch GSplus with a serial-capable configuration.

The workflow uses `appleiigs-project.env`, which sets:

```text
SOURCE=vintanetgs_workflow.c
OBJECT=vintanetgs.obj
PROGRAM=vintanetgs
DEST_NAME=VINTANETGS
FILE_TYPE=S16
AUX_TYPE=0
GSPLUS_BIN=/Volumes/MEDIA/Applications/GSplus.app/Contents/MacOS/GSplus
GSPLUS_CONFIG=gsplus-vintanetgs.kegs
```

## ORCA/C Workflow

The shared workflow is resolved relative to this project:

```text
../../../Devel_Ops/AppleIIGS/appleiigs-workflow.sh
```

The workflow compiles one source file with ORCA/C through Golden Gate `iix`, then links the executable:

```sh
iix compile vintanetgs_workflow.c keep=vintanetgs.obj
iix link vintanetgs.obj keep=vintanetgs
```

The import stage imports the executable and the configured companion file:

```text
VINTANETGS      S16  aux 0
VINTANETGS.CFG  TXT  aux 0
```

The GSplus emulator stage uses the repo-local `gsplus-vintanetgs.kegs` file.
Slot 1 / printer-port capture is configured as GSplus incoming TCP on
`127.0.0.1:6501`; `g_serial_mask[0]` remains zero for eight-bit binary data.
Use `./scripts/launch-vintanetgs-gsplus.sh` to start GSplus with this exact
configuration without rebuilding or importing.  Launching GSplus from Spotlight
uses the default `~/config.kegs` file and is not valid for VintaNetGS serial
testing unless that global config is manually updated.

After GSplus starts, verify the printer-port listener:

```sh
lsof -nP -iTCP:6501 -sTCP:LISTEN
```

Use `./scripts/capture-printer-serial.sh` for raw host capture, or run the live
host peer from the test-driver repo:

```sh
cd /Users/kevincarr/projects/VintaNetTestDriver
python3 vntest.py serial-peer --serial-port 6501
```

## TEXTUIGS Integration

The current Devel_Ops workflow compiles a single source file. VintaNetGS follows the same wrapper pattern used by TEXTUIGS:

- Canonical VintaNetGS headers remain in `include/`.
- Canonical VintaNetGS implementation remains in `src/`.
- TEXTUIGS forwarding adapters point back to `../TEXTUIGS`.
- `vintanetgs_workflow.c` includes TEXTUIGS implementation files exactly once.
- The TEXTUIGS test program is never included.

## Current Limitations

- Configuration is loaded from `VINTANETGS.CFG` in the current working directory, but there is no interactive setup editor yet.
- Raw serial transport is not enabled for normal app polling; it is available
  only through explicit serial diagnostics.
- No packet framing or VintaNet protocol implementation.
- No discovery or known-machine state.
- No INFO request/response.
- No routed commands or remote launch.
- No remote-control implementation.
- No GS/OS desktop GUI or Super Hi-Resolution graphics.

## Migration Phase Order

1. Development structure and UI shell
2. Configuration loading
3. Raw serial transport
4. Packet framing and communication
5. Discovery and known-machine state
6. INFO request/response
7. Routed commands and remote launch
8. VintaNetGS-specific remote control
9. Investigation of arbitrary GS/OS application control

Controlling another VintaNetGS instance means exchanging intentional VintaNet messages with a cooperating peer. Controlling arbitrary GS/OS applications is a separate investigation and must not be assumed to work until the Apple IIgs system interfaces and application behavior are documented.

## Configuration

VintaNetGS loads `VINTANETGS.CFG` from the current working directory. The format is DOS-style flat `KEY = VALUE` text with case-insensitive keys. Full-line comments beginning with `#` are ignored after leading whitespace is trimmed. Inline comments are not supported.

Supported keys:

| Key | Default | Validation |
| --- | --- | --- |
| `MACHINE` | blank | Blank is valid and means setup is needed; nonblank values are uppercased and clipped to the fixed buffer. |
| `NODE_ID` | `0` | Integer `0..65535`; `0` is reserved for future derived ID behavior. |
| `ROLE` | `ADMIN` | `ADMIN` or `SLAVE`, normalized uppercase. |
| `PORTS` | blank | Blank is valid and means setup is needed; otherwise comma-separated ports `1..4`, normalized without spaces. |
| `BAUD` | `2400` | `1200`, `2400`, or `9600`. |
| `DISCOVERY_WINDOW` | `30` | Integer `1..3600`; parsed now for later discovery work. |
| `DISCOVERY_REQUEST_COUNT` | `3` | Integer `1..99`; parsed now for later discovery work. |
| `DISCOVERY_REQUEST_INTERVAL_SECONDS` | `5` | Integer `1..3600`; parsed now for later discovery work. |
| `DISCOVERY_PRESENCE_INTERVAL_SECONDS` | `30` | Integer `1..3600`; parsed now for later discovery work. |
| `INFO_REFRESH_SECONDS` | `60` | Integer `1..3600`; parsed now for later INFO work. |
| `INFO_REQUEST_COUNT` | `3` | Integer `1..99`; parsed now for later INFO work. |
| `INFO_REQUEST_INTERVAL_SECONDS` | `5` | Integer `1..3600`; parsed now for later INFO work. |
| `ZMEXE` | `C:\PDZM\ZM.EXE` | Stored and normalized uppercase, but not used yet. |
| `ZMOPTIONS` | DOS default options | Stored and normalized uppercase, but not used yet. |

`[CAPABILITIES]` is recognized. Capability names and commands are parsed, normalized uppercase, stored in fixed arrays, and counted. Capabilities are not displayed as executable actions and are not used for INFO or launch behavior yet.

Missing `VINTANETGS.CFG` is not fatal. Defaults remain active and the UI reports `CONFIG DEFAULTS`. Invalid numeric values, invalid roles, invalid port lists, malformed non-comment lines, and capability overflow report `CONFIG ERROR` with a line number.

Unknown keys outside `[CAPABILITIES]` are ignored for DOS compatibility.

The current UI displays configuration status, machine name, ports, baud, and capability count. Blank machine or ports display as `SETUP NEEDED`. The save API exists in the configuration module, but the TEXTUIGS shell does not yet provide text entry for first-run setup.
