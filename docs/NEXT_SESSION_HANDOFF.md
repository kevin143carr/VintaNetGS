# Next Session Handoff

Codex thread id: `019fa36a-3a46-76f2-ae61-0559e89c2b93`

Last committed state:

```text
260343f Validate Lane A firmware TX smoke path
```

## Current Result

Lane A firmware TX is byte-transparent for the eight-byte smoke payload under
GSplus incoming TCP when using the configured `H` diagnostic.

Verified host capture:

```text
00 01 09 0A 0D 17 80 FF
```

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

Stop serial implementation work until the Lane A TX smoke path is confirmed on
real Apple IIgs hardware.  After real-hardware confirmation, the next narrow
phase should be RX validation.  Do not begin direct SCC, protocol framing,
discovery, INFO, routing, file transfer, or remote-control work yet.

