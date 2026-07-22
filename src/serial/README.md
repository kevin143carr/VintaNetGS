# Serial Transport Layer

This directory is reserved for future raw Apple IIgs serial transport.

The serial layer will eventually own the hardware or system-tool boundary for byte-oriented communication. It must expose a narrow transport API to higher layers without leaking register-level details into the network protocol code.

Not implemented in Phase 1:

- Serial port initialization
- Baud-rate configuration
- Interrupt handling
- Receive or transmit buffers
- DOS UART register compatibility code
