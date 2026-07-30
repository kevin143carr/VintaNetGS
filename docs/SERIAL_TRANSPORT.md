# Apple IIgs Serial Transport Investigation

## Decision

VintaNetGS now treats Apple IIgs serial transport as two explicit lanes.
Lane A uses the built-in serial firmware through the Pascal 1.1 entry table,
protected by the GS/OS Slot Arbiter.  A narrow ORCA/M assembly shim performs
the native-to-emulation transition.  The C backend provides application-owned,
nonblocking queues.  Lane B is reserved for a future direct-SCC transport, but
no SCC registers are touched until Apple IIgs Technical Note #18 and the Z8530
register contract are documented locally.

The firmware lane remains valuable and Apple-supported, but current GSplus
tests show that it must not be assumed byte-transparent.  It is retained to
gather byte-level evidence and to compare printer and modem firmware behavior.
Direct SCC access remains a future fallback only if the firmware path cannot
be made byte-faithful with documented firmware controls.

The firmware interface was originally selected because these local sources
were available:

1. The Apple IIgs Firmware Reference documents `PINIT`, `PREAD`, `PWRITE`,
   and `PSTATUS`, firmware buffering, and the extended queue interface.
2. Apple Technical Information Library article "Apple IIGS: 6502
   communications applications" specifies the Pascal entry-table layout
   and recommends the firmware instead of SCC programming.
3. The installed ORCA/M 2.0 manual documents loader segment kind `$12` as
   direct-page/stack storage in bank zero.  The installed ORCA/C runtime
   source documents the 256-byte SANE reservation and 4096-byte default C
   stack used by the shim segment layout.
4. Apple IIgs Technical Note #69 requires Slot Arbiter protection around
   slot-dependent access and says `FWEntry` must not be used for `$Cxxx`.
5. The available GS/OS image does not contain a raw serial character
   driver, and the available Text Tools/ORCA printer interfaces cannot
   provide transparent bidirectional nonblocking queues.

Direct SCC access still carries higher implementation risk because it must
respect GS/OS, AppleTalk ownership, interrupt behavior, SCC timing, and cleanup.
The current milestone therefore investigates the documented firmware Z command
before any SCC register access.

## Transport lanes

Lane A is the serial-firmware validation lane.  It owns the current `PINIT`,
`PREAD`, `PWRITE`, `PSTATUS`, queue, and diagnostics work.  Its acceptance
criterion is byte-faithful TX/RX for all 256 values on real hardware and a raw
host endpoint.

Lane B is the direct-SCC investigation lane.  It is currently only the
`vn_scc.c` no-hardware API stub.  No SCC registers are touched, receive
handling is not enabled, interrupts are not installed, protocol framing is not
added, and the application does not fallback automatically from Lane A.  The
application still performs no normal serial activation at startup.

## Slot-1 mapping and call contract

The Pascal table starts at offset `$0D` in each slot firmware page.  For
internal slot 1, `$C10D` through `$C110` hold the low address bytes for
`PINIT`, `PREAD`, `PWRITE`, and `PSTATUS`.  The shim reads the applicable
table byte and calls the resulting `$C1xx` address.  X is `$C1`, Y is `$10`
(the slot-number nibble required for slot 1), and the firmware is entered in
6502 emulation mode with bank and direct page zero.  The bridge saves and
restores the native stack, registers, processor state, data bank, direct
page, and prior slot configuration.

The firmware routines return with `RTS`, so the call instruction must also
execute in bank zero.  `vn_serial_fw.asm` is appended by the single-source
workflow and linked as an ORCA/M `$12` segment.  The segment contains a
256-byte SANE reservation, the small shim, and a full 4096-byte C stack.

`PSTATUS` request 1 reports whether input is available; request 0 reports
whether output can be accepted.  The transport calls `PREAD` or `PWRITE`
only after the corresponding ready result.  Each poll has fixed RX and TX
work budgets and therefore returns promptly even under continuous traffic.
In the manual probe, `PSTATUS` carry is readiness state, not an error:
`C=1` means input is available for RX status or output can accept a byte for
TX status; `C=0` means not ready.

## Lane A Z-mode notes

The Apple IIgs Firmware Reference documents serial firmware commands as bytes
embedded in the serial output flow.  It says commands are preceded by a command
character and optionally followed by a return character, and that the command
character is usually Control-I in printer mode.  The archived text is OCR-noisy
but the relevant passage appears at lines 8204-8219 of
`Apple_IIgs_Firmware_Reference_djvu.txt`:
`https://archive.org/stream/Apple_IIgs_Firmware_Reference/Apple_IIgs_Firmware_Reference_djvu.txt`.

The same reference documents command strings for baud rate, data format, and
parity.  It lists `nB` for baud selection, with `10` meaning 2400 baud, `nD`
for data bits and stop bits, and `nP` for parity.  The local setup commands
already use `10B`, `0D`, and `0P`; the disabled Z-mode experiment also used
`0N` before the existing formatting and flow-control disables to turn off line
formatting deliberately.  ORCA/C's local `misctool.h` also exposes the
matching BRAM serial settings `p1Baud`, `p1DtStpBits`, `p1Parity`,
`p1AddLine`, `p1Echo`, `p1Buffer`, and `p1XnfHndShk`, confirming these are
firmware-controlled printer-port settings.

The Firmware Reference documents `Z` as "Suppress control characters".  It
says the Z command causes all further commands to be ignored when transmitted
data contains bit patterns the serial firmware might mistake for control
characters.  It specifically documents `Control-I Z CR`, which is byte
sequence `09 5A 0D`, and says all tabbing and line formatting are disabled
after that command.  The same passage says command recognition is restored
only by initializing the serial firmware or by using SetModeBits.  The
SetModeBits table documents bit 23 as "Ignore commands in the output flow".
Those passages are in the archived Firmware Reference at lines 8696-8713 and
9444-9552.

The Z-mode diagnostic is currently disabled.  Both the first all-at-once
implementation and the later one-firmware-operation-per-keypress stepper
locked up or destabilized the GSplus diagnostic session before producing a
useful end-to-end byte-transparency result.  The active diagnostic baseline is
therefore the original Lane A raw smoke workflow: open slot-1 firmware with
`I`, then send the raw smoke bytes with `W`.  The Z-mode notes above are kept
only as evidence for why the `$09 $0A` loss may be firmware command parsing,
not as an enabled test path.

## Lane A mode-bit experiment

The current safer command-suppression experiment uses the Serial-Port Firmware
extended interface instead of sending `09 5A 0D` through `PWRITE`.  The
archived Apple IIgs Firmware Reference Chapter 5 documents the optional-control
routine dispatch convention at lines 9392-9427: read the optional-control
routine offset from `$CN12`, add it to `$CN00`, enter the resulting `$CNxx`
routine with DBR `$00`, and pass the command-list pointer as address low in
A, address middle in X, and address high in Y.  For slot 1, VintaNetGS reads
`$C112` and dispatches to `$C100 + offset`.

The same reference documents `GetModeBits` and `SetModeBits` at lines
9443-9552.  The `GetModeBits` command list used here is:

```text
03 00 rr rr mm mm mm mm
```

where byte 0 is the parameter count, byte 1 is command code `$00`, bytes 2-3
receive the result word, and bytes 4-7 receive the 32-bit `ModeBitImage`.
The `SetModeBits` command list is:

```text
03 01 rr rr mm mm mm mm
```

where byte 1 is command code `$01` and bytes 4-7 contain the input
`ModeBitImage`.  The reference says bit 0 is the least significant bit of the
lowest-addressed byte, so mode bit 23 is C mask `0x00800000UL`, byte 2 bit 7
of the four-byte mode image.  Bit 23 means `1 = Ignore commands in the output
flow`.  The reference also warns callers to preserve bits marked Preserve and
to disable interrupts across the Get/modify/Set sequence to avoid a race.

Because the diagnostic advances on separate keypresses, VintaNetGS does not
leave interrupts disabled between a visible `GetModeBits` step and a later
visible `SetModeBits` step.  Stage 4 instead uses one bounded assembly bridge
invocation that performs only `GetModeBits`, ORs bit 23, and calls
`SetModeBits` while interrupts are disabled inside that single invocation.
The original Stage 2 image is still retained for display and for final
restoration.  Restoration uses `SetModeBits` with the exact saved image and is
verified with a final `GetModeBits`; the normal success path does not restore
through `PINIT`.

The mode-bit stepper is entered from the byte-I/O diagnostics with `B`.
`O` performs the separate deliberate restore/verify action if normal
progression stops after the original image has been captured.  The stepper
redraws an in-flight marker before every firmware call.  The payload phase
uses the same split `PSTATUS` then single-byte `PWRITE` pattern as the
existing raw printer stepper and sends only:

```text
00 01 09 0A 0D 17 80 FF
```

Local success is displayed only as `LOCAL TX ACCEPTED/MODE RESTORED`.
Byte transparency still requires host TCP capture of the final payload in
order and unchanged.

The separate `H` diagnostic is the configured 8N1 retest.  It is a chunked
operator diagnostic, not a one-firmware-call-per-keypress localizer.  It uses
one initial `PINIT`, then the same mode-bit bridge: capture the original mode
image, clear bit 23, verify that output-flow command recognition is enabled,
send the setup stream, set bit 23, verify command suppression, send the
payload, restore the exact original mode image, and verify restoration.  The
setup and payload byte phases run as chunks from one `H` keypress each; each
individual `PSTATUS` and `PWRITE` still redraws an in-flight marker before
entering firmware and stops on wait, failure, or hang marker.  It does not send
the unsafe Z command.  The normal success path stops after verified
`SetModeBits` restoration; it does not perform automatic final `PINIT` because
GSplus can hang in `PINIT` after a successful payload capture.  The setup
stream is the existing documented diagnostic stream:

```text
09 31 30 42
09 30 44
09 30 50
09 43 44
09 58 44
09 46 44
09 4C 44
09 45 44
09 4D 44
09 42 45
```

The purpose of `H` is to test whether the prior `$80->$00` and `$FF->$7F`
capture came from running the mode-bit payload without first applying the
firmware's 2400 8N1 and formatting-disable setup.

Repeat capture showed why `H` must clear bit 23 before setup.  When the
previous run restored to an original image with bit 23 still set, the next
setup stream was transmitted to the host instead of being interpreted as
firmware commands:

```text
09 31 30 42 09 30 44 09 30 50 09 43 44 09 58 44
09 46 44 09 4C 44 09 45 44 09 4D 44 09 42 45
00 01 09 0A 0D 17 00 7F
```

That run preserved `$09 $0A` but still stripped the high bit from `$80 $FF`,
so command setup had not taken effect.  The corrected `H` sequence clears bit
23 and verifies it before setup, then sets bit 23 only for the payload.

A later chunked `H` test stopped at `67/87 GET ORIGINAL` and captured two
copies of the 31-byte setup stream with no payload.  That was an application
state-machine bug: the final setup write transitioned back to `GET ORIGINAL`
instead of continuing to `PREPARE BIT23`.  The setup-complete transition now
continues to `PREPARE BIT23`.

After that transition fix, `H` completed but still captured the setup stream
followed by `00 01 09 0A 0D 17 00 7F`.  This showed that clearing bit 23 is not
enough under GSplus to put the firmware into the state where setup bytes are
recognized as serial firmware commands.  The configured `H` diagnostic now
uses one initial `PINIT` before the mode-bit and setup sequence, but still
does not use `PINIT` as the success-path restoration step.

Final GSplus result after restoring the initial `PINIT` stage: `H` completed
locally and host TCP capture received exactly:

```text
00 01 09 0A 0D 17 80 FF
```

No setup bytes appeared in that capture.  This proves Lane A firmware TX is
byte-transparent for the eight-byte smoke payload under GSplus incoming TCP
when the diagnostic begins with `PINIT`, sends the setup stream while command
recognition is enabled, sets bit 23 for the payload, and restores the original
mode image with `SetModeBits`.

Previous GSplus result on 2026-07-27: the `B` stepper successfully completed
Stage 1 and displayed the slot-1 extended dispatch location.  Pressing `B`
again redrew `MODE STEP 2 GET ORIGINAL IN-FLIGHT` and entered the first
`GetModeBits` call, but the application/emulator crashed before returning to
C.  `VINTANETGS.LOG` ended with:

```text
MODE NEXT BEGIN step=2 stage=1
```

The mode-bit TCP capture contained zero bytes because payload transmission was
never reached.  The crash localized to the first extended-interface
`GetModeBits` call path, not to `PSTATUS`, `PWRITE`, payload transmission,
host capture, or restoration.

The immediate assembly defect found after that crash was in the
extended-interface bridge, not in the raw Lane A `PINIT`/`PSTATUS`/`PWRITE`
path.  The bridge now keeps the actual firmware `JSR` and both mode-bit
command lists in the ORCA/M `$12` bank-zero segment, enters that segment by
`JSL`, calls the optional-control routine by patched same-bank `JSR`, restores
the saved native stack before continuing native code, and reads the result
word from command-list offset `+2`.  The bridge also pulls one-byte `PHP` and
`PHB` diagnostic snapshots with the accumulator in 8-bit mode, avoiding native
stack imbalance before the firmware transition.  The 2026-07-27 linker map
shows `VN_FW_BANK` as segment 3, type `$12`, with
`vn_serial_fw_ext_invoke=$023A`, `vn_fw_ext_call=$0446`,
`vn_fw_get_mode_cmd=$051C`, and `vn_fw_set_mode_cmd=$0524` within that
segment.  Runtime retest is still required before claiming `GetModeBits`
works or that Lane A is byte-transparent.

Follow-up GSplus result on 2026-07-27 after the bridge fix: the `B` stepper
completed locally.  `VINTANETGS.LOG` showed original mode image `00000000`,
modified image `00800000`, payload accepted count 8, restore through
`SetModeBits`, and final restored image `00000000`.  Host TCP capture received:

```text
00 00 01 09 0A 0D 17 00 7F
```

The `$09 $0A` payload bytes arrived, so bit 23 suppresses firmware command
parsing in GSplus.  Byte transparency is still not claimed because the likely
payload region ended with `$00 $7F` instead of `$80 $FF`.  The `H` diagnostic
was added to retest the same bit-23 path after the existing 8N1 setup bytes.

Follow-up GSplus result on 2026-07-27 with `H`: host TCP capture received the
exact eight-byte payload:

```text
00 01 09 0A 0D 17 80 FF
```

This is byte-transparent under GSplus incoming TCP for the tested payload when
the firmware setup stream is applied before setting mode bit 23.  The
diagnostic then verified mode-bit restoration, but the later automatic final
`PINIT` hung at `MODE FINAL PINIT IN-FLIGHT`.  That final `PINIT` was removed
from the normal success path; Lane A firmware remains the primary transport
candidate, with real Apple IIgs hardware TX confirmation recorded below.

Repeatability note: after the first successful `H` run, backing out of the
diagnostic view and re-entering initially showed stale complete state, and a
fresh app run within the same GSplus process could hang at the old `I`/`PINIT`
open path.  The diagnostic now resets mode-bit state when entering the serial
diagnostics screen, restarts `H` when it is pressed after a complete or failed
`H` run, uses a visible initial `PINIT`, clears bit 23 before setup, does not
use final `PINIT`, and blocks the same-process `I` open path after `H` has
been used.  The configured `H` diagnostic is therefore run as `D`, `V`, `H`;
`I` remains the original raw `I`/`W` smoke-test open step and must not be used
immediately after `H` under GSplus without restarting GSplus.

## Controlled firmware probe

Normal serial activation is compile-time disabled while firmware invocation is
validated.  The serial diagnostics screen exposes a manual 39-step sequence:

1. Native assembly entry and return
2. Native-to-emulation-to-native transition
3. Slot Arbiter `$E10208` query with `A=$8000`
4. Slot Arbiter `$E10208` internal-port-1 request with `A=$0001`
5. Slot Arbiter `$E10208` external-slot-1 request with `A=$0009`
6. One `PINIT` through the ORCA/C Misc Tool `FWEntry` interface
7. Thirty-one individual `PWRITE` calls for the existing setup commands
8. One input `PSTATUS` and one output `PSTATUS`

Every `N` keypress presents an `IN FLIGHT` marker before making one call.  A
successful call advances one step; an arbiter or firmware error remains on the
same step, except the `$8000` query and external-slot-1 comparison steps are
informational and intentionally advance after recording A, X, Y, carry, and
arbiter error.  The internal-port-1 request remains a required gate before
firmware calls.  The request diagnostics save the current bit-encoded slot
configuration and restore it before returning to C.  The `PINIT` checkpoint
uses the documented `FWEntry` interface rather than the custom bank-zero
firmware shim.  `R` resets only the diagnostic sequence and invokes no
firmware.  The two `PSTATUS` checkpoints pass with carry either clear or set
as long as no firmware error bits are returned in X and no arbiter error is
reported.  The probe does not call `PREAD`, send payload data, open the
application transport, or enable continuous status polling unless later
firmware diagnostics are intentionally run.

## Emulator boundary

The active GS/OS build/import/launch workflow uses GSplus 1.38 because it
includes SCC emulation.  Older Clemens-based runs remain useful only as
startup, configuration, UI, queue-stability, and application-polling checks;
publicly documented Clemens releases do not emulate SCC serial communication.
Stable zero counters in Clemens show only that the polling path and
diagnostics remain idle without a backend.  They do not prove electrical
signaling, cabling, firmware interrupts, or byte-accurate printer-port
communication.

The repo-local `gsplus-vintanetgs.kegs` mounts the workflow disk image, uses
`APPLE2GS.ROM2`, maps GSplus SCC port 0 to channel A / slot 1 / printer port,
and maps GSplus SCC port 1 to channel B / slot 2 / modem port.  The current
capture setup uses GSplus incoming TCP mode for slot 1:

```text
g_serial_cfg[0] = 3
g_serial_device[0] =
g_serial_mask[0] = 0
```

GSplus serial configuration mode 3 opens an incoming TCP listener; slot 1 uses
TCP port 6501 and slot 2 would use TCP port 6502.  `g_serial_mask[0]` must stay
zero because the raw smoke payload includes eight-bit bytes `$80` and `$FF`.
The previous PTY/socat bridge experiment was removed from the launch path
because the PTY bridge did not stay alive reliably across GSplus open/close
behavior and added an unneeded capture variable.  Slot 2 is not configured for
the same endpoint in this test.

After launching GSplus, verify the slot-1 listener:

```sh
lsof -nP -iTCP:6501 -sTCP:LISTEN
```

Start binary-safe capture before pressing `W` in the VintaNetGS diagnostics:

```sh
./scripts/capture-printer-serial.sh
```

The manual fallback is:

```sh
rm -f /tmp/vintanetgs-printer.bin
nc 127.0.0.1 6501 > /tmp/vintanetgs-printer.bin
wc -c /tmp/vintanetgs-printer.bin
xxd -g 1 -u /tmp/vintanetgs-printer.bin
```

The script uses macOS `nc -d` so capture can run from a noninteractive shell
without stdin EOF closing the TCP connection; it does not use telnet mode and
does not transform received bytes.

After capture starts, verify the TCP connection:

```sh
lsof -nP -iTCP:6501
```

The host capture must show the final raw smoke payload exactly as
`00 01 09 0A 0D 17 80 FF` before Lane A can be called byte-transparent.
Any setup or firmware bytes before that payload must be reported separately.

Observed GSplus incoming-TCP result on 2026-07-27: VintaNetGS logged
`IO OPEN RESULT ok=1 status=1 err=0` and
`RAW W RESULT accepted=8 status=1 err=0`, but host capture received only:

```text
00 01 0D 17 80 FF
```

The capture file was `/tmp/vintanetgs-printer.bin` with byte count 6.  The
expected `$09 $0A` payload bytes were absent, so byte transparency is not
claimed.  This reproduces the earlier evidence without PTY/socat in the path.

The current GSplus and real-hardware probe result is deliberately limited.
Native entry/return and native-to-emulation-to-native return pass with zeroed
result registers.  Manually mapping the bank-$01 GS/OS vectors before
`JSL $01FCBC` fixed the earlier hard hang, but the query-only call now returns
carry set with `A=$0010`, `X=$807E`, `Y=$0000`, and arbiter error `$0010` on
both GSplus and real hardware.  The `$E10208` vector returns the same values,
so vector mapping is no longer the leading suspect.  This known `$8000` result
is treated as informational in the manual probe so the next checkpoints can
test internal-port-1 and external-slot-1 request behavior through `$E10208`
only, with save/restore around each request.  The external-slot-1 request is
also treated as informational so its failure does not block the next controlled
`PINIT` checkpoint.  Local Byte Works source exposes `FWEntry` as the
application-facing firmware entry interface, so the controlled `PINIT`
checkpoint now uses `FWEntry` with the Control Panel still selecting internal
slot 1 as the printer port.  Later manual Byte I/O diagnostics reached
`PSTATUS` and `PWRITE`; those results are recorded below.  No direct SCC
serial activity has been validated yet.

## Current handoff

The latest manual probe run reached the firmware path.  Step 5,
`ARB EXT1 $0009`, reports failure with `A=$0010`, `X=$807E`, `Y=$00FF`,
`C=1`, and arbiter error `$0010`; this is the external-slot-1 comparison and
is not the current blocker when the Control Panel is intentionally using the
internal printer port.  Step 6, `PINIT`, passes through the ORCA/C `FWEntry`
path, proving the internal slot-1 printer firmware initialization entry point
can return successfully from VintaNetGS.

The step 39 `PSTATUS TX` failure was a diagnostic interpretation error: carry
is readiness, not failure.  With the current code, the `PSTATUS` checkpoints
fail only when X reports firmware error bits or the arbiter error field is
nonzero.  The `$8000` query and external-slot-1 `$0009` comparison are shown
as informational results instead of blocking failures.

The current controlled byte-I/O test is firmware-byte comparison, not VintaNet
protocol work.  The diagnostics screen keeps normal serial activation disabled,
but provides an explicit byte-I/O view: `I` opens the slot-1 diagnostic
transport, `W` sends raw `00 01 09 0A 0D 17 80 FF` directly through configured
slot-1 printer `PWRITE`, `U` sends the same raw pattern after only slot-1
`PINIT` with no baud or formatting setup commands, `L` advances the configured
slot-1 Lane A byte stepper through open/setup, TX status checks, and one raw
byte per keypress, `M` advances one slot-2 modem checkpoint at a time through
`PINIT`, TX status checks, and the same raw smoke bytes without firmware setup,
`S` queues the same pattern through the experimental escaped TX path, `P`
performs bounded manual polling, and `C` closes the slot-1 diagnostic
transport.  The experimental Z-mode diagnostic is disabled after GSplus
lockups; host PTY verification remains manual and external.  The diagnostics
loop does not continuously poll serial firmware while idle.

The app writes a short ASCII runtime log to `VINTANETGS.LOG` in the same GS/OS
directory as the app.  The log is overwritten at app start and each diagnostic
line is opened, written, and closed immediately so a later firmware hang should
still leave the last completed marker on disk.  After quitting GSplus, export
the log from the workflow image with:

```sh
appcom -e /Users/kevincarr/projects/AppleIIGS/System601HD.hdv VINTANETGS.LOG /tmp/VINTANETGS.LOG
```

For Lane A host capture, start the slot-1 TCP capture before pressing `W`:

```sh
./scripts/capture-printer-serial.sh
```

It connects to `127.0.0.1:6501`, writes raw bytes to
`/tmp/vintanetgs-printer.bin`, and prints a byte count plus `xxd -g 1 -u`
hexadecimal dump after capture stops.

The first logged run showed `I` returned `OPEN`, `W` locally accepted all eight
raw smoke bytes, and the next hang occurred after `SERIAL DIAG EXIT CLOSE`.
That places the GSplus lockup in the close-time restoring `PINIT`, not in the
raw smoke write.  Close-time firmware restore is disabled during this diagnostic
phase so the app can exit and leave a readable log; open/configure still uses
the slot-1 firmware initialization path.

The `U` no-setup printer path called slot-1 `PINIT`, accepted one raw byte, and
the host received only `00`.  The configured Lane A `L` stepper then showed
that `00`, `01`, `0D`, `17`, `80`, and `FF` reached the host.  For `$09` and
`$0A`, VintaNetGS displayed `L RAW PASS` after `PWRITE`, but the host received
no bytes.  This may be firmware command parsing rather than loss below the
firmware boundary: `$09` is the documented printer-mode command character, and
the following `$0A` can plausibly be consumed as command-stream input.  The
Z-mode path is disabled after GSplus lockups; the configured `H` diagnostic is
the accepted safer experiment for byte-transparent TX validation.

The modem stepper redraws `BYTE IO` with an `Mxx ...` marker before each
firmware call so a freeze identifies the blocking slot-2 operation.  The
current slot-2 modem path passes `PINIT`, but repeated TX status checks remain
at `M TX WAIT` under GSplus.  The previous modem setup experiment froze on the
first setup `PWRITE $09` immediately after a passing slot-2 `PINIT`, so setup
commands are intentionally skipped until raw slot-2 status/write behavior is
understood.  Raw diagnostic TX bypasses the application TX queue and the `$09`
escape state machine; it is only for controlled transport validation.

The escaped path remains unproven under GSplus because the latest post-sweep
smoke exposed `09 17` bytes on the wire.  Transmit and receive results must be
compared against the host endpoint or real hardware peer before claiming serial
success.

Real Apple IIgs hardware TX result on 2026-07-30: CoolTerm hex mode at
2400,N,8,1 captured the configured `H` diagnostic setup stream followed by
the exact eight-byte smoke payload:

```text
09 31 30 42 09 30 44 09 30 50 09 43 44 09 58 44
09 46 44 09 4C 44 09 45 44 09 4D 44 09 42 45
00 01 09 0A 0D 17 80 FF
```

The setup stream corresponds to `.10B.0D.0P.CD.XD.FD.LD.ED.MD.BE`; the final
line is the smoke payload.  This confirms Lane A printer-firmware TX for the
current payload and baud on real hardware.

Real Apple IIgs hardware RX result on 2026-07-30: with the Control Panel
printer device set to 2400,N,8,1, unlimited line length, buffering on, echo
off, both CR/LF transforms off, and DCD, DSR/DTR, and XON/XOFF handshakes off,
CoolTerm line-mode `hello` arrived as:

```text
68 65 6C 6C 6F 0D 0A
```

CoolTerm Send String in hex mode then transmitted the smoke payload:

```text
00 01 09 0A 0D 17 80 FF
```

The VintaNetGS `P` diagnostic displayed the exact same bytes.  This confirms
Lane A printer-firmware RX for the smoke payload and current baud on real
hardware.  The next diagnostic broadens RX validation to:

```text
00 01 02 03 04 05 06 07 08 09 0A 0D 10 17 80 FF
```

The first real-hardware RX16 run reported `0014/0016`, with no
parity/framing/overrun errors displayed, and captured:

```text
00 01 02 03 04 05 06 07 08 0A 0D 10 80 FF
```

The expected bytes `$09` and `$17` were absent.  The final narrow serial
diagnostic before moving to TLV/packet library work was `Z`, checking whether
those command-character values are consistently consumed by the printer
firmware RX path:

```text
09 17 08 09 0A 10 17 18
```

The real-hardware `Z` run passed:

```text
RX BYTES: 00000008
BYTE IO:  RXCMD TEST
RESULT:   RX PASS
BYTES:    0008/0008
POLL:     0008 ERR:0/0/0
D0:       09 17 08 09 0A 10 17 18
NOTE:     RX MATCH
```

Lane A is therefore sufficient for TLV/packet library work.  The RX16
`0014/0016` result is retained as a follow-up validation note, not a current
packet-layer blocker.

Real hardware or a serial-capable emulator must still verify:

- 1200, 2400, and 9600 baud setup, with 2400 as the current configuration
- 8N1 timing and peer compatibility
- all 256 byte values, especially printer firmware command byte `$09`
- parity, framing, and overrun reporting under induced faults
- sustained queue behavior and overflow counters

Full 256-byte and sustained serial behavior are not yet proven.
