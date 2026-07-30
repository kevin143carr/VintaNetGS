# VintaNet Protocol Baseline

VintaNetGS protocol work is based on the DOS VintaNet project:

```text
/Users/kevincarr/projects/MSDOS/C/VINTANET
```

The primary behavior reference is:

```text
VINTANET_PROTOCOL.md
```

The packet and TLV compatibility references are:

```text
VNPROTO.H
VNPROTO.CPP
VNTLV.H
VNTLV.CPP
```

VintaNetGS may adapt implementation details for ORCA/C and Apple IIgs memory
constraints, but it must preserve documented VintaNet protocol behavior.
Protocol behavior must not be invented from unused constants alone.

The currently active tested protocol messages are:

```text
DISCOVERY_ANNOUNCE
INFO_REQ
INFO_RESP
LAUNCH_REQ
```

Other message IDs exist in `VNPROTO.H`, including `ACK`, `NAK`, and `ERROR`,
but they are not active tested behavior unless the corresponding DOS behavior
is documented first.

Next packet-layer work should start with DOS-compatible
`DISCOVERY_ANNOUNCE` packet construction and parsing.  Its payload uses TLVs
for local identity and routing metadata:

```text
NODE_ID
NODE_NAME
ROLE
CAP_FLAGS
INFO_REVISION
TTL
HOP_COUNT
KNOWN_NODE
```

Do not start automatic INFO polling, routing, file transfer, launch execution,
or remote-control behavior as part of packet/TLV validation.
