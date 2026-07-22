# Apple IIgs Serial Transport Investigation

## Decision

VintaNetGS uses the built-in printer-port firmware through the Pascal 1.1
entry table, protected by the GS/OS Slot Arbiter.  A narrow ORCA/M assembly
shim performs the native-to-emulation transition.  The C backend provides
application-owned, nonblocking queues.

This is the highest supported interface found locally that satisfies raw
bidirectional byte transfer and availability polling:

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

Direct SCC access was rejected.  It would duplicate firmware ownership,
interrupt handling, and buffering, and it is lower in Apple's documented
preference order.

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

## Controlled firmware probe

Normal serial activation is compile-time disabled while firmware invocation is
validated.  The serial diagnostics screen exposes a manual 37-step sequence:

1. Native assembly entry and return
2. Native-to-emulation-to-native transition
3. Slot Arbiter request and restore
4. One `PINIT`
5. Thirty-one individual `PWRITE` calls for the existing setup commands
6. One input `PSTATUS` and one output `PSTATUS`

Every `N` keypress presents an `IN FLIGHT` marker before making one call.  A
successful call advances one step; an arbiter or firmware error remains on the
same step.  `R` resets only the diagnostic sequence and invokes no firmware.
The probe does not call `PREAD`, send payload data, open the application
transport, or enable continuous status polling.

## Emulator boundary

Clemens is suitable for the GS/OS build/import/launch workflow and UI tests,
but its publicly documented releases do not emulate SCC serial
communication.  Stable zero counters in Clemens show only that the polling
path and diagnostics remain idle without a backend.  They do not prove
electrical signaling, cabling, firmware interrupts, or byte-accurate
printer-port communication.

GSplus 1.38 includes SCC emulation and maps slot 1 to serial port 0.  The
initial probe setup uses incoming TCP port 6501 with full 8-bit data, while the
IIgs Control Panel remains set to Printer Port with AppleTalk disabled.

The current GSplus probe result is deliberately limited.  Native entry/return
and native-to-emulation-to-native return pass with zeroed result registers.
The next checkpoint beeps and remains `IN FLIGHT` at the first Slot Arbiter
call (`JSL $01FCBC`).  This occurs before `PINIT`, `PWRITE`, `PSTATUS`, or SCC
serial activity.  Adding `CLD` at both native 16-bit entry points satisfies the
Technical Note #69 decimal-mode requirement but did not change this GSplus
result.  A real IIgs test is required to determine whether the stopping point
is specific to GSplus or remains a shim problem.

Real hardware or a serial-capable emulator must verify:

- Mini-DIN-8 printer-port transmit and receive
- 1200, 2400, and 9600 baud setup, with 2400 as the current configuration
- 8N1 timing and peer compatibility
- all 256 byte values, especially printer firmware command byte `$09`
- parity, framing, and overrun reporting under induced faults
- sustained queue behavior and overflow counters

No physical serial success is claimed by either emulator workflow.
