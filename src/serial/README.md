# Serial Transport Layer

VintaNetGS uses the Apple IIgs built-in serial firmware for the printer
port.  The public transport in `vn_serial.c` is independent of TEXTUIGS,
configuration parsing, and the VintaNet packet layer.  `vn_serial_fw.c` and
`vn_serial_fw.asm` form the only native/emulation-mode boundary.

## Selected interface

The selected interface is the documented Pascal 1.1 serial firmware
protocol for internal slot 1:

- `$C10D`: low byte of the `PINIT` address
- `$C10E`: low byte of the `PREAD` address
- `$C10F`: low byte of the `PWRITE` address
- `$C110`: low byte of the `PSTATUS` address

The shim reads the selected operation's low address byte from this table,
loads X with `$C1` and Y with `$10`, and invokes the resulting `$C1xx`
routine in 6502 emulation mode.  Every call requests internal port 1 from
the GS/OS Slot Arbiter at `$01FCBC`, then restores the prior bit-encoded
slot configuration.  These are documented table locations and calling
conventions, not guessed firmware vectors.

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
command character.  No carriage-return or line-feed translation is enabled,
no received byte is echoed, and payload bytes cannot be interpreted as
firmware commands.

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

The `T` command runs the deterministic ring-buffer wrap/full test before
reporting the existing protocol test result.  Clemens can verify startup,
configuration, UI, queue stability, and application polling.  Publicly
documented Clemens releases do not emulate SCC serial communications, so
actual printer-port TX/RX must be validated on a real Apple IIgs or a
serial-capable emulator.

For hardware testing, connect a binary-safe peer, open the serial
diagnostic screen, and verify receive counts and recent bytes with patterns
that include `00`, `01`, `09`, `0A`, `0D`, `17`, `80`, and `FF`.  Then enqueue the
same pattern from a dedicated transport test build and confirm the peer
receives it byte-for-byte.  This milestone does not enqueue any bytes from
the application and does not transmit VintaNet frames.
