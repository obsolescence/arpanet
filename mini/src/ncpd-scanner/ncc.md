# Claude Code generated temporary arpanet scanner

Basically, a hacked version of ncpd that sends out messages to see which nodes respond. Will report 71 when a node does not respond, or 70 if the IMP itself does not respond.

Not to be used for anything serious


# Network Control Center (NCC) Daemon - Summary of Work

This document summarizes the modifications and additions made to transform the original `ncpd` daemon into the `ncc` (Network Control Center) daemon, a specialized tool for monitoring the status of ARPANET IMPs.

## Core Changes and Additions:

1.  **Project Duplication and Renaming:**
    *   The entire `src` directory (containing the original `ncpd` daemon code) was copied to a new directory named `ncc`. This establishes `ncc` as an independent project.
    *   The main daemon source file `ncc/ncp.c` was renamed to `ncc/ncc.c` to better reflect its new purpose.

2.  **Makefile Updates:**
    *   The `Makefile` within the `ncc` directory was modified to build an executable named `ncc` (instead of `ncpd`) from `ncc.c` and its dependencies.

3.  **IMP Status Tracking (`ncc.c`):**
    *   **Global Status Arrays:** Two new global arrays, `static uint8_t imp_status[256];` and `static uint64_t imp_last_seen[256];`, were added to `ncc.c`. These arrays are designed to store the last observed IMP message type and the `time_tick` (an internal timestamp) for each of the 256 potential IMPs on the network.
    *   **Real-time Status Updates:** The `process_imp` function in `ncc.c` (the central point for handling all incoming messages from the IMP) was modified. It now records the message `type` and the `time_tick` into the `imp_status` and `imp_last_seen` arrays, respectively, for the originating IMP of each received message. This provides a real-time, low-level view of IMP activity.

4.  **Network Topology Configuration and Probing (`ncc.c`):**
    *   **Topology File Parsing:** A new function, `parse_topology_config(const char *filename)`, was implemented in `ncc.c`. This function reads a simple text-based configuration file (e.g., `arpanet.conf`) which lists the IMP numbers expected to be part of the network.
    *   **Known IMPs Storage:** Global arrays `static uint8_t known_imps[MAX_IMPS];` and `static int num_known_imps = 0;` were added to store the parsed topology. `parse_topology_config` is called from `main` during daemon startup.
    *   **Proactive IMP Probing:** A periodic probing mechanism was integrated into `ncc.c`'s main event loop. Every 5 seconds, the `ncc` daemon iterates through the list of `known_imps` and sends an `ncp_rst()` (NCP Reset) message to each. This proactive polling ensures that `imp_status` and `imp_last_seen` are regularly updated, even if an IMP is idle or unresponsive (which would trigger an `IMP_DEAD` message from the local IMP).

5.  **Client-Daemon Status API:**
    *   **New `WIRE_IMP_STATUS` Command:** A new command, `#define WIRE_IMP_STATUS 15`, was added to `ncc/wire.h`. This command facilitates communication between client applications (like the `ncc-panel`) and the `ncc` daemon for status requests.
    *   **`wire_check` Validation:** The `wire_check` function in `ncc/wire.h` was updated to correctly validate the expected sizes of `WIRE_IMP_STATUS` requests (requesting status for a specific IMP) and their corresponding replies (containing the status and timestamp).
    *   **Daemon-side Handler (`app_imp_status` in `ncc.c`):** A new function, `static void app_imp_status(void);`, was implemented in `ncc.c`. This function processes incoming `WIRE_IMP_STATUS` requests. It retrieves the requested IMP's `imp_status` and `imp_last_seen` from the global arrays and sends this information back to the requesting client. The `application()` function's `switch` statement was updated to dispatch `WIRE_IMP_STATUS` requests to this handler.
    *   **Client-side Library Function (`ncp_imp_status` in `libncp.c`):** The `ncc/ncp.h` file received a new function prototype, `extern int ncp_imp_status(int imp, int *status, uint64_t *last_seen);`, and its implementation was added to `ncc/libncp.c`. This function provides a convenient API for client applications to query the `ncc` daemon for an IMP's current status and last-seen timestamp.

## Result:

The `ncc` daemon has been successfully transformed into a robust, specialized network monitoring component. It now actively tracks the health and activity of specified ARPANET IMPs, making this real-time data available to client applications via a dedicated API. This lays the groundwork for creating rich, interactive monitoring tools like the `ncc-panel`.
