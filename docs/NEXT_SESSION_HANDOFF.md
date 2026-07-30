# Next Session Handoff

Codex thread id: `019fa36a-3a46-76f2-ae61-0559e89c2b93`

Last committed state:

```text
260343f Validate Lane A firmware TX smoke path
```

## Current Result

Lane A firmware TX is byte-transparent for the eight-byte smoke payload under
GSplus incoming TCP when using the configured `H` diagnostic.

Verified GSplus host capture:

```text
00 01 09 0A 0D 17 80 FF
```

Real Apple IIgs hardware also verified the configured `H` diagnostic TX path
on 2026-07-30 with CoolTerm hex mode at 2400,N,8,1.  The capture showed the
complete setup stream followed by the exact smoke payload:

```text
09 31 30 42 09 30 44 09 30 50 09 43 44 09 58 44
09 46 44 09 4C 44 09 45 44 09 4D 44 09 42 45
00 01 09 0A 0D 17 80 FF
```

Real Apple IIgs hardware also verified RX for the same smoke payload on
2026-07-30.  CoolTerm Send String in hex mode transmitted:

```text
00 01 09 0A 0D 17 80 FF
```

VintaNetGS displayed the exact same bytes through the `P` diagnostic poll.
Before the Control Panel settings were corrected, line-mode text input showed
partial lines such as `68 65 0A`; with printer-port settings at 2400,N,8,1,
unlimited line length, buffering on, echo off, LF transforms off, and all
handshakes off, `hello` was received as `68 65 6C 6C 6F 0D 0A`.

The working diagnostic sequence is:

```text
D
V
H
```

Do not press `I` before `H`.  `H` performs a visible initial `PINIT`, sends the
setup stream while command recognition is enabled, sets mode bit 23 for the
payload, sends the payload, restores the original mode image with
`SetModeBits`, and verifies restoration.  It does not perform a final `PINIT`.

## Important Evidence

Raw `I` then `W` locally accepts all eight bytes, but host capture loses
`09 0A`, matching printer-firmware command parsing.

Mode-bit `B` allowed `09 0A` through but, without setup, captured:

```text
00 00 01 09 0A 0D 17 00 7F
```

Chunked `H` without initial `PINIT` emitted setup bytes as data and still
masked high-bit payload bytes.  Therefore the initial `PINIT` is required
before setup under GSplus.

## Next Step

Lane A TX/RX smoke is confirmed on real Apple IIgs hardware.  The next narrow
phase is a broader RX16 hardware diagnostic using:

```text
00 01 02 03 04 05 06 07 08 09 0A 0D 10 17 80 FF
```

First real-hardware RX16 run showed:

```text
00 01 02 03 04 05 06 07 08 0A 0D 10 80 FF
```

The count was `0014/0016`, with no parity/framing/overrun errors displayed.
Expected bytes `09` and `17` were absent.

The final command-byte RX diagnostic with `Z` used:

```text
09 17 08 09 0A 10 17 18
```

Real Apple IIgs hardware result:

```text
RX BYTES: 00000008
BYTE IO:  RXCMD TEST
RESULT:   RX PASS
BYTES:    0008/0008
POLL:     0008 ERR:0/0/0
D0:       09 17 08 09 0A 10 17 18
NOTE:     RX MATCH
```

Move on to TLV/packet library work unless packet integration later requires a
specific serial follow-up.  The main `T` diagnostic runs the TLV/packet
self-test suite and should be the next GSplus check.

Do not begin direct SCC, discovery, INFO, routing, file transfer, or
remote-control work yet.
