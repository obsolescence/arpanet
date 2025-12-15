# Analysis of the NCP Daemon (ncpd)

## High-Level Overview

`ncpd` is a daemon program that implements the historic ARPANET Network Control Program (NCP). Its primary function is to act as a bridge, translating requests from modern local applications into the protocol spoken by a simulated ARPANET Interface Message Processor (IMP), which functions as the network's router.

The daemon is built on a classic event-driven architecture. The main loop uses the `select()` system call to simultaneously monitor two different communication channels: one for local applications and one for the IMP. This allows it to react to events from either side without blocking. Internally, `ncpd` maintains a complex state machine, tracking the status of every connection (e.g., opening, established, closing) as it negotiates the intricacies of the NCP protocol.

---

## How Applications Connect to `ncpd`

The connection between local client applications (like `ncp-finger`, `ncp-telnet`) and the `ncpd` daemon is a form of Inter-Process Communication (IPC) that works as follows:

1.  **Mechanism**: The interface is a **UNIX Domain Socket**. This type of socket uses a file on the filesystem as its address instead of an IP address and port.

2.  **Socket Path**: The exact path to the socket file is determined by the `NCP` environment variable. When `ncpd` is started with `NCP=ncp2`, it creates a socket file named `ncp2` in the current directory.

3.  **Communication Style**: `ncpd` creates a datagram socket (`SOCK_DGRAM`), meaning communication is message-oriented, not a continuous stream.

4.  **Protocol**: Client applications send binary messages to this socket. The format and meaning of these messages are defined by opcodes in the `wire.h` header file. For example, to open a connection, an application sends a message with the `WIRE_OPEN` opcode, followed by the required parameters like destination host and socket.

5.  **Server-Side Handling**: Inside `ncpd`, the `application()` function is responsible for reading these messages from the socket. It uses a large `switch` statement to identify the opcode (`WIRE_OPEN`, `WIRE_LISTEN`, etc.) and calls the corresponding function (e.g., `app_open()`, `app_listen()`) to handle the request.

---

## How `ncpd` Connects to the IMP

The connection between `ncpd` and the external IMP simulator is the low-level network link.

1.  **Initialization**: The connection is established during startup by the `imp_init(argc, argv)` function. This function takes the command-line arguments passed to `ncpd` (e.g., `localhost 22001 22002`) to determine how to connect to the IMP process. This suggests it uses a standard TCP or UDP network socket.

2.  **Abstraction**: All direct interaction with the IMP is handled by a set of functions prefixed with `imp_*` (e.g., `imp_send_message`, `imp_receive_message`). This neatly separates the NCP logic from the specifics of the IMP link.

3.  **Receiving Data**: The main `select()` loop monitors the file descriptor associated with the IMP connection. When data is available, `imp_receive_message()` is called to read it. The raw message is then passed to `process_imp()`, which interprets the IMP-level protocol (e.g., checking for leader errors, host status messages, or regular data packets) and acts accordingly.

4.  **Sending Data**: To send a message to another host on the network, `ncpd` constructs an NCP packet and sends it via the `send_ncp()` and `send_imp()` functions, which ultimately use `imp_send_message()` to transmit the data over the socket connected to the IMP.
