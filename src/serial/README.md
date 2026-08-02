# Serial Transport Layer

VintaNetGS currently keeps two serial transport lanes.  Lane A uses the
Apple IIgs built-in serial firmware.  `vn_serial.c` is independent of TEXTUIGS,
configuration parsing, and the VintaNet packet layer.  `vn_serial_fw.c` and
`vn_serial_fw.asm` form the only native/emulation-mode boundary.  Slot-2 modem
firmware is exposed only through controlled raw diagnostics.  Lane B is the
future direct-SCC path; `vn_scc.c` is currently a no-hardware stub so the SCC
API boundary exists without touching SCC registers.

## Selected interface

The selected normal transport interface is the documented Pascal 1.1 serial
firmware protocol for internal slot 1:

- `$C10D`: low byte of the `PINIT` address
- `$C10E`: low byte of the `PREAD` address
- `$C10F`: low byte of the `PWRITE` address
- `$C110`: low byte of the `PSTATUS` address

The shim reads the selected operation's low address byte from this table,
loads X with `$C1` and Y with `$10`, and invokes the resulting `$C1xx`
routine in 6502 emulation mode.  Every normal call requests internal port 1
from the GS/OS Slot Arbiter through the application-safe `$E10208` vector,
then restores the prior bit-encoded slot configuration.  The modem diagnostic
uses the same call shape with slot-2 values: `$C20D` through `$C210`, X
`$C2`, Y `$20`, and internal-port-2 arbiter request `$0002`.  These are
documented table locations and calling conventions, not guessed SCC vectors.

The ORCA/C and Golden Gate headers expose `FWEntry`, but Apple IIgs
Technical Note #69 explicitly prohibits using it for `$Cxxx` peripheral
firmware.  The installed GS/OS image has no bidirectional raw serial
character driver.  TEXTUIGS and ORCA shell printer output also do not
provide raw, nonblocking receive and queue-status operations.  A small
ORCA/M bridge is therefore used.  Pascal firmware returns with `RTS`, so
the bridge is linked in the loader-managed bank-zero direct-page/stack
segment.  Its layout reserves the first 256 bytes required by ORCA/C's
SANE startup, places the 185-byte shim next, and retains a full 4096-byte C
stack above it.  It does not access SCC registers.

## Configuration and binary data

`PINIT` initializes the selected slot-1 printer firmware.  Setup is sent
through `PWRITE` using the firmware commands below:

- `10B` for 2400 baud (`8B` for 1200, `14B` for 9600)
- `0D` for 8 data bits and 1 stop bit
- `0P` for no parity
- `CD`, `XD`, `FD`, `LD`, `ED`, and `MD` to disable line formatting,
  XON/XOFF, keyboard input, automatic line feed, echo, and incoming-LF
  masking
- `BE` to enable firmware buffering

The printer port uses byte `$09` (Control-I) as its command character.
Ordinary payload bytes are written unchanged.  To transmit payload byte
`$09`, the TX state machine changes the command character to `$17`, writes
`$09` as data, and changes the command character back.  This is the
documented method for the built-in ports, which do not transmit a doubled
command character.  The escaped TX path is still experimental under GSplus:
the latest post-sweep smoke showed `09 17` escape bytes on the host side, so
this behavior is not yet proven.  No carriage-return or line-feed translation
is enabled, no received byte is echoed, and payload bytes cannot be
intentionally interpreted as firmware commands.

## Queues and errors

The application owns static 1024-byte RX and TX rings for the full process
lifetime.  `vn_serial_write` accepts only the bytes that fit and returns
immediately.  `vn_serial_poll` performs at most 32 receive operations and
32 transmit operations per call.  It checks `PSTATUS` before `PREAD` or
`PWRITE`; it does not delay for character timing or wait for either queue
to drain.

The firmware's parity, framing, and overrun status bits are counted.
Application-ring overflow counts, queue depths, high-water marks, total
payload bytes, and the most recent error are also retained.  `PINIT` is
called on close to return the firmware to its initialized state.

## Hardware prerequisites

- Set slot 1 to the internal printer port in the IIgs Control Panel.
- Disable AppleTalk on the printer port.
- Use a Mini-DIN-8 cable or adapter wired for the target device's RS-422 or
  compatible serial interface.  A modem-style cable and a printer-style
  cable are not interchangeable in every setup.
- Configure the peer for the same baud rate, 8 data bits, no parity, one
  stop bit, and no software flow control.

## Testing

The main-screen `T` command runs the deterministic ring-buffer wrap/full test
before reporting the TLV/packet self-test result.  Startup and normal polling
use the slot-1 printer firmware when the network transport is configured.
Shutdown performs bounded RX/TX cleanup and clears local queues before
returning to GS/OS.  It does not call close-time `PINIT` because that
synchronous firmware path can lock GSplus while exiting.

The serial diagnostics screen provides a manual 39-step firmware probe.  `N`
runs exactly one checkpoint, `R` resets the sequence without calling firmware,
and `V` switches between counters and probe results.  The sequence tests the
native assembly return, emulation-mode return, a `$E10208` Slot Arbiter query,
safe `$E10208` internal-port-1 and external-slot-1 request diagnostics,
`PINIT` through ORCA/C `FWEntry`, each of the 31 setup `PWRITE` bytes, RX
`PSTATUS`, and TX `PSTATUS`.
The screen is presented with `IN FLIGHT` before each call so a crash identifies
the exact operation.  Errors hold the current step instead of advancing, except
the informational `$8000` query step advances after recording A, X, Y, carry,
and arbiter error so the request diagnostics can be reached.  The external
slot-1 comparison is also displayed as informational because the current
validated path intentionally uses the internal printer port.
For the `PSTATUS` checkpoints, carry is readiness state rather than an error:
RX `C=1` means input is available, and TX `C=1` means output can accept a byte.

The byte-I/O view now includes packet-specific diagnostics above the raw
printer-port path.  `X` sends one DOS-compatible `DISCOVERY_ANNOUNCE` reference
packet generated by the direct TLV test helper.  `K` polls for the same
reference packet, compares the full byte sequence, extracts the packet, and
parses its TLVs.  These controls are serial diagnostics only; they do not start
live discovery polling, routing, or normal application serial activation.

For GSplus 1.38 testing, use the project workflow's repo-local
`gsplus-vintanetgs.kegs`.  It mounts `System601HD.hdv`, uses
`APPLE2GS.ROM2`, maps slot 1 / port 0 and slot 2 / port 1 to
`/tmp/vintanetgs-gsplus-serial`, and selects full 8-bit data for mutually
exclusive diagnostics.  The workflow starts a `socat` pseudo-tty pair before
launch; the host-side test endpoint is `/tmp/vintanetgs-host-serial`.  In the
IIgs Control Panel, set slot 1 to Printer Port with AppleTalk disabled and
slot 2 to Modem Port before probing.

The observed GSplus 1.38 and real-hardware runs pass the native return and
emulation return checkpoints.  Manually mapping the bank-$01 GS/OS vectors
before `JSL $01FCBC` fixed the previous hard hang, but the query-only call now
returns carry set with `A=$0010`, `X=$807E`, `Y=$0000`, and arbiter error
`$0010` on both systems.  The `$E10208` vector returns the same values, so
vector mapping is no longer the leading suspect.  This known `$8000` result is
treated as informational so the next checkpoints can test internal-port-1 and
external-slot-1 request behavior through `$E10208` only, with save/restore
around each request.  The guarded firmware shim now uses the passing
internal-port-1 request path, but the next controlled `PINIT` checkpoint uses
the documented `FWEntry` interface directly.  Later manual Byte I/O diagnostics
reached firmware `PSTATUS` and `PWRITE`; those results remain diagnostics only,
not a proven transport.  No direct SCC operation has been validated.

Older Clemens-based workflow runs can verify startup, configuration, UI, queue
stability, and application polling, but publicly documented Clemens releases
do not emulate SCC serial communications.  Actual printer-port TX/RX must be
validated on a real Apple IIgs or a serial-capable emulator.

For hardware testing, connect a binary-safe peer, open the serial diagnostic
screen, and use `V` to select the byte-I/O view.  `I` opens the slot-1
diagnostic transport, `W` sends raw `00 01 09 0A 0D 17 80 FF` directly through
configured slot-1 printer `PWRITE`, `U` sends the same raw pattern after only
slot-1 `PINIT` with no baud or formatting setup commands, `L` advances the
configured slot-1 Lane A byte stepper through open/setup, TX status checks, and
one raw byte per keypress, `M` advances one slot-2 modem checkpoint at a time
through `PINIT`, TX status checks, and the same raw smoke bytes without
firmware setup, `S` queues the same pattern through the unproven escaped TX
path, `P` performs bounded manual polling, and `C` closes the slot-1 diagnostic
transport.  The attempted Z-mode diagnostic is disabled after GSplus lockups.
The modem stepper redraws
`BYTE IO` with an `Mxx ...` marker before each firmware call so a freeze
identifies the blocking slot-2 operation.  The
previous modem stepper froze on the first setup `PWRITE $09` immediately after
a passing slot-2 `PINIT`, so setup commands are intentionally skipped until raw
slot-2 status/write behavior is known.  The current slot-2 modem path passes
`PINIT`, but repeated TX status checks remain at `M TX WAIT` under GSplus.

Raw diagnostic TX bypasses the application TX queue and `$09` escape state
machine and is only for controlled transport validation.  The `U` no-setup
printer path accepted and sent only `00`.  The configured Lane A `L` stepper
sent `00`, `01`, `0D`, `17`, `80`, and `FF` to the host.  For `$09` and `$0A`,
VintaNetGS displayed `L RAW PASS` after `PWRITE`, but the host received no
bytes.  Those bytes may be consumed as printer-mode firmware commands because
`$09` is the documented Control-I command character.  The attempted Z-mode
diagnostics locked up or destabilized GSplus, so that path is disabled and the
active Lane A baseline remains the raw smoke workflow.  The diagnostics screen
does not continuously poll serial firmware while idle.  This milestone sends
only manual diagnostic bytes and does not transmit VintaNet frames.
