# Next Session Handoff

Last base commit before this handoff:

```text
44fefe7 Document hardware serial TLV pass
```

## Current Result

VintaNetGS now has the first live direct VintaNet interface on Apple IIgs:

- TEXTUIGS dashboard with local status, known machines, selected-machine details,
  packet status, and quiet partial redraws.
- Live direct discovery over printer port 1 using the VintaNet packet/TLV layer.
- Manual `W` starts a bounded WARM announce window.
- Startup uses HOT discovery when configured.
- Incoming new nodes or new discovery sessions can wake a bounded WARM response
  window when currently COLD.
- Safe direct `KNOWN_NODE` gossip is included and parsed.
- Enter on a selected machine can send an on-demand `INFO_REQ`.
- INFO responses are direct only, use the raw-safe packet path, and use a basic
  required-field payload to avoid Apple IIgs printer-firmware `$09` command-byte
  problems.
- Up/down machine navigation is display-only and cancels any pending INFO retry.

Real hardware status on 2026-08-01:

- Woz, the Apple IIgs, appeared on the VintaNet DOS machine list.
- The Compaq 286 serial card was replaced because the old UART could not keep up.
- After the UART replacement and quiet-navigation changes, the current build is
  working fairly well on the real VintaNet.

## Verified Build

Latest transferred artifacts:

```text
/Volumes/AppleShare/VintageComputers/Apple IIGS/transfer/VINTANETGS
/Volumes/AppleShare/VintageComputers/Apple IIGS/transfer/VINTANETGS.CFG
/Volumes/AppleShare/VintageComputers/Apple IIGS/transfer/System601HD.hdv
```

Transfer timestamp:

```text
2026-08-01 13:35
```

Transferred sizes:

```text
VINTANETGS      208,344 bytes
VINTANETGS.CFG      447 bytes
System601HD.hdv 33,554,432 bytes
```

The transfer copies were byte-compared against the local build artifacts.

Verification completed:

```text
./build.sh build
Protocol self-test: SUMMARY total=33 passed=33 failed=0
RUN_SECONDS=3 ./build.sh run-gsos
```

`run-gsos` completed build, import, and GSPlus launch.  Escalation was required
only because the workflow updates the sibling `System601HD.hdv` disk image.

## Important Boundaries

- Do not modify sibling `TEXTUIGS` or `Devel_Ops` from this repo.
- Keep `vintanetgs_workflow.c` as the single compiler source.
- Keep discovery announcements separate from INFO request/response behavior.
- Do not add automatic INFO polling.
- Do not add remote-control code until serial and packet communications are
  proven on real hardware.
- VintaNetGS does not launch applications yet.  Capability names are displayed,
  but selecting a capability and pressing Enter reports `Launch not implemented.`

## Tomorrow

Start with real-hardware observation, not new features:

- Confirm the screen remains quiet while using up/down navigation.
- Confirm pressing Enter is visibly the only action that starts an INFO request.
- Confirm Woz responds to an INFO request from Franky after direct request
  traffic reaches the IIgs.
- Watch for repeated `INFO_REQ` from DOS peers; if they repeat, add duplicate
  suppression for inbound INFO request/response handling before adding launch.
- If INFO is stable, plan the next phase as a minimal direct `LAUNCH_REQ` /
  `LAUNCH_RESP` between cooperating VintaNet machines.  Do not implement routed
  launch or arbitrary GS/OS application control first.
