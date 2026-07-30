# VintaNetGS Migration Plan

The DOS VintaNet codebase is a reference for behavior and responsibilities. VintaNetGS must preserve documented protocol behavior where applicable, but Apple IIgs implementation details must be designed for ORCA/C, GS/OS, and the available Apple IIgs hardware interfaces.

## Phase 1: Development Structure and UI Shell

Scope:

- Establish the ORCA/C project directory structure.
- Consume TEXTUIGS from the sibling repository.
- Build one S16 executable named `vintanetgs`.
- Draw a deterministic 40-column by 24-row development shell.

Explicit exclusions:

- Configuration loading.
- Serial communication.
- Packet framing.
- Discovery.
- INFO handling.
- Routed commands.
- Remote control.

Acceptance criteria:

- `./build.sh build` succeeds.
- `vintanetgs`, `vintanetgs.obj.a`, and `vintanetgs.obj.root` are produced.
- The shell waits for Escape or `Q` and restores the display before exit.
- TEXTUIGS and Devel_Ops remain unchanged.

Dependencies on earlier phases:

- None.

## Phase 2: Configuration Loading

Scope:

- Define fixed-size configuration records.
- Load local machine identity and serial settings from an Apple IIgs-appropriate source.
- Validate defaults and invalid input behavior.
- Parse all DOS configuration fields into VintaNetGS-owned fixed-size storage.
- Count `[CAPABILITIES]` entries without executing them.

Explicit exclusions:

- Serial I/O.
- Packet communication.
- Discovery or INFO messages.
- Interactive first-run setup editing until text-entry support exists.
- Capability execution.

Acceptance criteria:

- Configuration can be loaded or defaults can be selected deterministically.
- Invalid configuration cannot overflow buffers or enter partial state.
- UI reports configuration status without implying active communication.
- The UI displays machine name, ports, baud, and capability count.
- Blank machine and ports remain valid setup-needed values.

Dependencies on earlier phases:

- Phase 1 UI shell and module structure.

Status:

- Implemented as `include/vn_config.h` and `src/config/vn_config.c`.
- Runtime file: `VINTANETGS.CFG` in the current working directory.
- Next milestone: raw Apple IIgs serial transport.

## Phase 3: Raw Serial Transport

Scope:

- Implement byte-oriented serial initialization, read, write, and shutdown.
- Keep hardware-specific code isolated in the serial layer.
- Use fixed-size buffers only.

Explicit exclusions:

- VintaNet packet parsing.
- Discovery.
- INFO behavior.
- Remote-control actions.

Acceptance criteria:

- Serial open and close paths are deterministic.
- Byte send and receive paths can be tested independently.
- No DOS UART register code is ported directly.

Dependencies on earlier phases:

- Phase 2 configuration values for serial settings.

Status:

- Lane A, the Apple IIgs serial-firmware path, has proven TX/RX for the
  eight-byte smoke payload on real Apple IIgs hardware at 2400,N,8,1.  The
  focused RX command-byte diagnostic also passed with `0008/0008` and
  `09 17 08 09 0A 10 17 18` received exactly.
- Broader RX16 validation received `0014/0016`; expected bytes `$09` and
  `$17` were absent in that run.  This remains a follow-up validation note,
  not a blocker for TLV/packet library work.
- Lane B, a future direct-SCC path, currently exists only as an isolated API
  stub; no SCC registers are touched yet.
- Phase 4 packet framing may proceed using the existing TLV/packet self-test
  path and the explicit serial packet diagnostics.  Full 256-byte and
  sustained serial validation remain later serial hardening tasks.

## Phase 4: Packet Framing and Communication

Scope:

- Implement VintaNet packet framing above raw serial transport.
- Add packet validation and fixed-size parse buffers.
- Keep framing separate from discovery and command semantics.

Explicit exclusions:

- Automatic INFO polling.
- Remote-control actions.
- Arbitrary GS/OS application control.

Acceptance criteria:

- Valid packets are accepted.
- Invalid or oversized packets are rejected safely.
- Packet send and receive behavior matches documented DOS protocol behavior.

Dependencies on earlier phases:

- Phase 3 raw serial transport.

Status:

- Started.  The main `T` diagnostic runs the TLV/packet self-test suite without
  enabling discovery, INFO polling, routing, file transfer, or remote control.
- Further packet work must follow `docs/PROTOCOL_BASELINE.md`, which anchors
  VintaNetGS packet/TLV behavior to the DOS VintaNet implementation and
  protocol document before serial packet transmission is added.
- DOS-compatible discovery and explicit INFO payload build/parse helpers are
  the first network-layer checkpoint; they remain offline self-test behavior
  only.
- The serial diagnostics add manual `X`/`K` `DISCOVERY_ANNOUNCE` packet tests
  above the proven byte-I/O path.  These are still diagnostics, not live
  discovery polling or routing.
- Real Apple IIgs hardware verified `K` receiving the 68-byte raw
  `DISCOVERY_ANNOUNCE` packet from CoolTerm as a binary file on 2026-07-30:
  VintaNetGS reported `PKT RX PASS`, `0068/0068`, and `DISCOVERY OK`.
  The next implementation step is the real serial packet layer, keeping the
  `X`/`K` diagnostics as the known-good hardware regression path.

## Phase 5: Discovery and Known-Machine State

Scope:

- Implement discovery announcements.
- Maintain fixed-size known-machine state.
- Preserve machine-name addressing and fallback behavior.

Explicit exclusions:

- INFO request/response.
- Routed commands.
- Remote control.

Acceptance criteria:

- Discovery announcements are distinct from INFO requests.
- Known-machine state has deterministic capacity limits.
- Address fallback behavior is documented and tested.

Dependencies on earlier phases:

- Phase 4 packet framing and communication.

## Phase 6: INFO Request/Response

Scope:

- Implement explicit INFO request and response behavior.
- Document corresponding DOS behavior before changing protocol semantics.
- Keep INFO separate from discovery announcements.

Explicit exclusions:

- Automatic INFO polling unless separately documented and accepted.
- Routed remote launch.
- Remote control.

Acceptance criteria:

- INFO requests are sent only through explicit behavior.
- INFO responses update only the intended known-machine fields.
- Missing or malformed INFO packets fail safely.

Dependencies on earlier phases:

- Phase 5 discovery and known-machine state.

## Phase 7: Routed Commands and Remote Launch

Scope:

- Implement routed command messages.
- Add remote launch behavior for cooperating VintaNetGS peers.
- Preserve addressing and fallback rules.

Explicit exclusions:

- Arbitrary GS/OS application control.
- Keyboard or screen remote control.

Acceptance criteria:

- Routed command behavior is documented against DOS source behavior.
- Remote launch commands are accepted only when addressed correctly.
- Unsupported commands fail visibly and safely.

Dependencies on earlier phases:

- Phase 6 INFO request/response.

## Phase 8: VintaNetGS-Specific Remote Control

Scope:

- Investigate and implement remote control between cooperating VintaNetGS instances.
- Keep remote actions above the command and packet layers.
- Maintain clear operator feedback in the text UI.

Explicit exclusions:

- Unproven control of arbitrary GS/OS applications.
- DOS interrupt, TSR, BIOS, or Borland-specific approaches.

Acceptance criteria:

- Remote-control messages are authenticated by protocol state available to the project.
- The receiver can reject unsupported actions cleanly.
- The UI distinguishes local state from remote-peer state.

Dependencies on earlier phases:

- Phase 7 routed commands and remote launch.

## Phase 9: Arbitrary GS/OS Application Control Investigation

Scope:

- Research whether arbitrary GS/OS application control is possible and appropriate.
- Document Apple IIgs system interfaces, limitations, and risks.
- Decide whether this belongs in VintaNetGS.

Explicit exclusions:

- Shipping implementation before the investigation is accepted.
- Assumptions based on DOS TSR or BIOS behavior.

Acceptance criteria:

- Findings are documented with Apple IIgs-specific evidence.
- A recommendation is made before implementation begins.
- Any prototype remains isolated from stable VintaNetGS peer-control code.

Dependencies on earlier phases:

- Phase 8 VintaNetGS-specific remote control.
