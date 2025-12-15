# Analysis of NCP Client Applications

This document provides a high-level overview and detailed operational description of the client applications designed to interact with the `ncpd` daemon.

---

## `ncp-discard`

**High-Level Description:**
The `ncp-discard` application is a server that implements the "discard" protocol. It listens on a specific NCP socket, accepts incoming connections, reads all data sent by the client, and simply throws it away.

**Detailed Operation:**
1.  **Initialization:** The application first initializes its connection to the `ncpd` daemon by calling `ncp_init()`. It checks for errors to ensure `ncpd` is running and the `NCP` environment variable is set.
2.  **Socket Selection:** It listens on socket 9 by default (`DISCARD_SOCKET`), but a different socket can be specified using the `-p` command-line option.
3.  **Listening:** It calls `ncp_listen()`, which blocks until a remote client requests a connection to the specified socket. Upon a successful connection, `ncpd` provides a new connection identifier.
4.  **Data Loop:** The server enters an infinite loop where it repeatedly calls `ncp_read()` on the connection. This function receives data sent by the client.
5.  **Discarding Data:** The content of the buffer holding the received data is never used. The application simply prints a message to `stderr` confirming the number of octets received and then loops to read again.
6.  **Termination:** The loop terminates when `ncp_read()` returns a value less than or equal to zero, which indicates that the client has closed its end of the connection.

---

## `ncp-echo`

**High-Level Description:**
The `ncp-echo` application implements both a client and a server for the "echo" protocol. The server listens for a connection and echoes back any data it receives. The client connects to a server, sends data from its standard input, and prints the server's echoed response to its standard output.

**Detailed Operation (Server Mode `-s`):**
1.  **Initialization:** The server initializes its connection to `ncpd` with `ncp_init()`.
2.  **Listening:** It calls `ncp_listen()` on a given socket (defaulting to 7, `ECHO_SOCKET`) and waits for a client to connect.
3.  **Echo Loop:** Once a connection is established, it enters a loop. Inside the loop, it calls `ncp_read()` to receive data from the client.
4.  **Responding:** Immediately after a successful read, it calls `ncp_write()` to send the exact same data back to the client over the same connection.
5.  **Termination:** The server process for that connection ends when the client closes the connection (`ncp_read()` returns <= 0).

**Detailed Operation (Client Mode `-c`):**
1.  **Initialization:** The client initializes its connection to `ncpd` with `ncp_init()`.
2.  **Connecting:** It calls `ncp_open()` to establish a connection with a remote host and socket specified on the command line.
3.  **I/O Loop:** The client enters a loop that performs the following actions:
    a. Reads a chunk of data from standard input (file descriptor 0).
    b. Sends that data to the echo server using `ncp_write()`.
    c. Waits for the server to send the data back, reading it with `ncp_read()`.
    d. Writes the echoed data to standard output (file descriptor 1).
4.  **Termination:** The loop continues until there is no more data to read from standard input. Finally, it calls `ncp_close()` to terminate the connection.

---


## `ncp-finger`

**High-Level Description:**
The `ncp-finger` application is a client for the "finger" protocol. It connects to a finger server on a remote host, sends a query (typically a username), and prints the server's informational response to the console.

**Detailed Operation:**
1.  **Arguments:** The program takes a remote host address and an optional user string as command-line arguments.
2.  **Initialization:** It connects to the local `ncpd` daemon via `ncp_init()`.
3.  **Opening Connection:** It calls `ncp_open()` to establish a connection to the specified host on the standard finger socket (port 117).
4.  **Sending Query:** It constructs a command string. If a user was specified, the string is `"[user]\r\n"`; otherwise, it's just `"\r\n"`. This query is sent to the server using `ncp_write()`.
5.  **Receiving Response:** The client makes a single call to `ncp_read()`, reading the server's entire response into a buffer.
6.  **Output:** It writes the contents of the reply buffer directly to standard output.
7.  **Closing:** The client terminates the session by calling `ncp_close()`.

---


## `ncp-finser`

**High-Level Description:**
The `ncp-finser` application is a minimal finger *server*. It is designed to demonstrate how a server-side application works. It listens for a connection on the finger socket, reads the incoming query, and sends back a hard-coded response that includes the text of the original query.

**Detailed Operation:**
1.  **Initialization:** The server initializes its connection to `ncpd` using `ncp_init()`.
2.  **Listening:** It calls `ncp_listen()` on the finger socket (117), waiting for a client to connect.
3.  **Reading Query:** Once a connection is established, it calls `ncp_read()` to receive the query sent by the finger client.
4.  **Formulating Response:** It prepares a pre-formatted response string that includes the text it just received from the client (e.g., "Sample response... Data from client was: '[query]' ").
5.  **Sending Response:** The server sends this formatted string back to the client using `ncp_write()`.
6.  **Closing:** It immediately calls `ncp_close()` to end the connection.

---


## `ncp-gateway`

**High-Level Description:**
`ncp-gateway` is a sophisticated utility that acts as a bidirectional bridge between a standard TCP/IP network and the ARPANET NCP network. It can be configured in two modes: forwarding a TCP connection to NCP, or forwarding an NCP connection to TCP.

**Detailed Operation:**
The gateway's core logic relies on creating dedicated reader and writer child processes (using `fork()` and `pipe()`) to handle NCP communication, while the main process uses `select()` to multiplex I/O between the TCP and NCP sides.

**Mode 1: TCP to NCP (`-T <port> <ncp_host> <ncp_socket>`)**
1.  **TCP Listener:** The gateway starts a TCP server on the specified `<port>` using `inet_server()` and waits for a connection with `inet_accept()`.
2.  **NCP Connector:** Once a TCP client connects, the gateway calls `ncp_open()` to establish a connection to the target `<ncp_host>` and `<ncp_socket>`.
3.  **Transport Bridge:** It calls the internal `transport()` function, which enters a `select()` loop. This loop monitors both the TCP socket and the pipe connected to the NCP `reader` process.
    - Data arriving on the TCP socket is read and written to the NCP `writer` process's pipe, which then sends it over the NCP network via `ncp_write()`.
    - Data arriving from the NCP `reader` process's pipe (read from the NCP network via `ncp_read()`) is written to the TCP socket.
4.  **Termination:** The bridge is torn down when either side closes the connection.

**Mode 2: NCP to TCP (`-N <ncp_socket> <tcp_host> <tcp_port>`)**
1.  **NCP Listener:** The gateway calls `ncp_listen()` on the specified `<ncp_socket>` and waits for an incoming NCP connection.
2.  **TCP Connector:** Once an NCP client connects, the gateway calls `inet_connect()` to establish a TCP connection to the target `<tcp_host>` and `<tcp_port>`.
3.  **Transport Bridge:** The `transport()` function is called to bridge the two connections in the same manner as the TCP-to-NCP mode.

---


## `ncp-ping`

**High-Level Description:**
`ncp-ping` is a network diagnostic tool analogous to the modern `ping` utility. It sends a low-level NCP echo request (an "are you there" message) to a remote host and measures the time it takes to receive a reply, confirming reachability and network latency.

**Detailed Operation:**
1.  **Arguments:** It parses command-line arguments for the target `host`, the number of pings to send (`-c`), and the interval between them (`-i`).
2.  **Initialization:** It initializes the connection to `ncpd` with `ncp_init()`.
3.  **Ping Loop:** The application enters a loop that runs for the specified `count`.
    a. **Start Time:** It records the current time using `gettimeofday()`.
    b. **Send Echo:** It calls `ncp_echo()`, a specific library function that sends a low-level NCP control message for echo testing. This is distinct from the echo protocol used by `ncp-echo`.
    c. **Handle Reply:** `ncp_echo()` blocks until a reply is received or an error occurs. It returns specific negative values for different failure conditions (e.g., IMP unreachable, host down).
    d. **End Time:** It records the time again upon receiving the reply.
    e. **Calculate and Print:** It calculates the round-trip time in milliseconds and prints a status line including the host, sequence number, and time.
    f. **Wait:** It sleeps for the specified interval before the next iteration.

---


## `ncp-telnet`

**High-Level Description:**
`ncp-telnet` provides both a client and a server for the Telnet protocol over NCP, enabling remote terminal sessions. It is a complex application that handles I/O multiplexing, Telnet option negotiation, and raw terminal mode. It supports multiple Telnet protocol variants (old, new, binary).

**Detailed Operation (Client Mode `-c`):**
1.  **Initialization:** It initializes `ncpd` with `ncp_init()` and establishes a connection to the target host and Telnet socket (1 or 23) using `ncp_open()`.
2.  **Process Forking:** Like the gateway, it forks `reader` and `writer` processes to handle `ncp_read()` and `ncp_write()` asynchronously, communicating with the main process via pipes.
3.  **Option Negotiation:** It sends initial Telnet option negotiation commands (e.g., `DO SUPPRESS-GO-AHEAD`) to the server via the `writer` process.
4.  **Terminal Mode:** It sets the local terminal to raw mode using `tty_raw()` to allow character-by-character input.
5.  **I/O Multiplexing:** The main process enters a `select()` loop, monitoring two file descriptors: standard input (for user keystrokes) and the `reader` pipe (for data from the server).
    - **User Input:** Keystrokes from the user are read and sent to the server via the `writer` pipe. A special escape character (`^]`) is used to quit.
    - **Server Data:** Data from the server is read from the `reader` pipe and passed to a processing function (`process_new`, `process_old`, etc.). This function interprets Telnet commands (like `IAC`, `EC`) and writes regular character data to standard output.
6.  **Termination:** On exit, it restores the terminal to its original state (`tty_restore()`) and closes the NCP connection.

**Detailed Operation (Server Mode `-s`):**
1.  **Listening:** The server listens on the Telnet socket for an incoming connection using `ncp_listen()`.
2.  **Process Forking:** It forks `reader` and `writer` child processes.
3.  **Shell Execution:** It uses a `tty_run("sh", ...)` helper function to launch a new shell process (`/bin/sh`) attached to a pseudo-terminal (PTY).
4.  **I/O Multiplexing:** The server enters a `select()` loop, monitoring two file descriptors: the PTY of the shell and the `reader` pipe from the client.
    - **Client Data:** Data from the client is read via the `reader` pipe, processed for Telnet commands, and then written to the shell's PTY, simulating user input.
    - **Shell Data:** Output from the shell is read from its PTY and written to the client via the `writer` pipe.
5.  **Termination:** The session ends when the client disconnects or the shell process terminates.
