# Configuration Layer

This layer owns VintaNetGS configuration records, defaults, text parsing, validation, file loading, and file saving. It does not draw to the screen and does not touch serial hardware or packet state.

## Runtime File

VintaNetGS loads this file from the current working directory:

```text
VINTANETGS.CFG
```

The GS/OS image should place the file beside the imported `VINTANETGS` application when runtime configuration testing is needed.

## Format

The format is DOS-style flat text:

```text
KEY = VALUE
```

Full-line `#` comments and blank lines are ignored. Inline comments are not supported. Keys are case-insensitive. Values are normalized uppercase for this milestone.

`[CAPABILITIES]` is recognized. Capability entries are parsed and counted but not executed.

Unknown keys outside `[CAPABILITIES]` are ignored for DOS compatibility. Malformed non-comment lines report a configuration error with the failing line number.

## Supported Fields

- `MACHINE`
- `NODE_ID`
- `ROLE`
- `PORTS`
- `BAUD`
- `DISCOVERY_WINDOW`
- `DISCOVERY_REQUEST_COUNT`
- `DISCOVERY_REQUEST_INTERVAL_SECONDS`
- `DISCOVERY_PRESENCE_INTERVAL_SECONDS`
- `INFO_REFRESH_SECONDS`
- `INFO_REQUEST_COUNT`
- `INFO_REQUEST_INTERVAL_SECONDS`
- `ZMEXE`
- `ZMOPTIONS`
- `[CAPABILITIES]` entries

Blank `MACHINE` and blank `PORTS` are valid. They indicate that first-run setup is needed. The interactive setup editor is deferred until TEXTUIGS has suitable text-entry support.

## Deferred

- Serial-port hardware access
- Packet framing
- Discovery execution
- INFO requests and responses
- Capability execution
- Interactive setup editing
- Remote launch and remote control
