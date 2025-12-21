# NCC (Network Control Center) – Program Summary

## Overview

**NCC** is a real-time ARPANET network monitoring daemon that visualizes IMP status on a map displayed in the terminal. It probes IMPs periodically and displays their responses in real time using VT-100 terminal positioning.

---

## Purpose

- Monitor ARPANET IMP nodes by sending periodic probe messages
- Display real-time network status as a geographic map in the terminal
- Show message types and reason codes for DEAD hosts
- Provide live statistics (cycle count, active IMPs, message count, elapsed time)

---

## Architecture

### Core Components

#### 1. IMP Layer (`imp.c`, `imp.h`)
- Low-level UDP communication with IMP simulator
- Message framing with H316 protocol
- Sequence number tracking
- Ready / RFNM (Ready For Next Message) flow control

#### 2. NCP Layer (`ncc.c`)
- Network Control Protocol implementation
- Connection management
- Message type handling (NOP, RST, ECO, ERP, etc.)
- Application interface (commented out – for future WebSocket use)

#### 3. Display System (`ncc.c`)
- VT-100 terminal control
- Geographic coordinate mapping
- Real-time status updates
- Dynamic terminal size adaptation

#### 4. Configuration Parser (`ncc.c`)
- Topology file parsing
- Port assignment extraction
- Coordinate mapping

---

## Configuration File Format

**File:** `arpanet-topology.conf`

### SECTION 1: IMP Topology

```
IMP <number> <name>
  connected to: <imp1> <imp2> <imp3> <imp4>
```

**Example**
```
IMP 1 UCLA
  connected to: 2 3 0 0
```

---

### SECTION 3: Port Assignments

```
hi1 <tx_port> <rx_port> host0 <hostname>
```

**Example**
```
hi1 20321 20322 host0 PARC-MAXC
```

---

### SECTION 4: Node Coordinates

```
IMP <number> <x> <y> #<name>
```

**Example**
```
IMP 32 45.5 12.3 #XEROX
```

---

## Display System

### Terminal Layout

```
Row 1:  [Blank]
Row 2:  Cycle: 5 | Active: 12/35 | Msgs: 47 | Time: 23s   [Status bar]
Row 3:  [Start of IMP map]
...
Rows 3–24: IMP geographic map (21 rows tall, 72 columns wide)
```

---

### Display Positioning Math

For a terminal with **H** rows:

- `status_line_row = 2`
- `display_offset = terminal_height - 22`
- Top IMP at `y=20`:
  ```
  row = (20 - 20) + 1 + offset = 1 + offset
  ```

Examples:
- 24-line terminal → offset = 2 → top IMP at row 3
- 40-line terminal → offset = 18 → top IMP at row 19

---

### Coordinate Transformation

**From config to screen:**

1. **X scaling**  
   Config: `0–60` → Screen: `0–72`
   ```
   screen_col = (int)(config_x * 1.2 + 0.5) + 1
   ```

2. **Y inversion**  
   Geographic south at top → bottom
   ```
   screen_row = (int)(20.0 - config_y + 0.5) + 1 + display_offset
   ```

---

### Display Format

Each IMP occupies **2 rows**:

```
Row N:     XEROX    (IMP name, truncated to 6 chars)
Row N+1:   32-5     (IMP# - MessageType)
           32-70    (IMP# - MessageType + ReasonCode for DEAD)
           32--     (Before first message received)
```

---

## Message Flow

### Startup Sequence

1. Parse command-line arguments (IMP number, optional config file)
2. Search for config file in `./`, `../`, `../../` if not specified
3. Parse topology (SECTION 1, 3, 4)
4. Detect terminal height via `ioctl(TIOCGWINSZ)`
5. Calculate display positioning
6. Initialize IMP connection with UDP ports
7. Clear screen with VT-100 (`\033[2J\033[H`)
8. Draw initial map (all IMPs show `NN--`)
9. Set host ready bit
10. Enter main loop

---

### Main Loop (Every Second)

- `select()` with 1-second timeout

**If IMP data arrives:**
- `imp_receive_message()`
- `process_imp()`
  - Extract type, source_imp, reason (if DEAD)
  - `update_imp_message()` *(real-time update)*
  - `update_status_line()`
  - `fflush(stdout)`

**Every 10 seconds:**
- Increment `probe_cycle`
- Clear screen (if cycle > 1)
- Redraw entire map
- Send RST probes to all known IMPs
- Update status line

---

### Real-Time Updates

- Messages update immediately as they arrive
- Each IMP response updates its display position only
- VT-100 cursor positioning avoids redrawing others
- Creates a “fill-in” effect as responses arrive

---

## Data Structures

### IMP Information

```c
typedef struct {
  uint8_t number;           // IMP number (1–255)
  char name[32];            // IMP name (e.g., "UCLA")
  uint8_t connected_imps[4];// Connected IMP numbers
  int num_connections;      // Count of connections
  float x;                  // X coordinate (0–60)
  float y;                  // Y coordinate (0–20)
} imp_info_t;
```

---

### Host Interface

```c
typedef struct {
  int tx_port;              // UDP port to send to IMP
  int rx_port;              // UDP port to receive from IMP
  char hostname[256];       // IMP hostname
} host_interface_t;
```

---

### Global State Arrays

```c
static uint8_t  imp_status[256];     // Message type for each IMP
static uint64_t imp_last_seen[256];  // Timestamp of last message
static uint8_t  imp_reason[256];     // Reason code for DEAD
```

---

## IMP Message Structure

### UDP Packet Format (`imp.c`)

| Bytes | Description |
|------:|-------------|
| 0–3   | `"H316"` magic |
| 4–7   | 32-bit sequence number |
| 8–9   | 16-bit word count |
| 10–11 | Flags (`FLAG_READY`, `FLAG_LAST`) |
| 12+   | IMP message payload |

---

### IMP Leader Format (`ncc.c`)

```
packet[0] = flags << 4 | type
packet[1] = source IMP number
packet[2] = link number
packet[3] = id << 4 | subtype/reason
packet[4+] = payload
```

---

## Message Types (IMP Layer)

| Type | Meaning |
|----:|---------|
| 0  | REGULAR – Regular data |
| 1  | ER_LEAD – Leader error |
| 2  | DOWN – IMP going down |
| 3  | BLOCKED – Link blocked |
| 4  | NOP – No operation |
| 5  | RFNM – Ready for next message |
| 6  | FULL – Link table full |
| 7  | DEAD – Host unreachable |
| 8  | ER_DATA – Data error |
| 9  | INCOMPL – Incomplete transmission |
| 10 | RESET – Reset |
| 15 | NEW – New IMP |

---

### DEAD Message Reason Codes

| Code | Meaning |
|----:|---------|
| 0 | IMP cannot be reached |
| 1 | Host is not up |
| 3 | Communication administratively prohibited |

---

## Key Functions

### Configuration Parsing
- `parse_section1()` – Extract IMP topology
- `parse_section3_for_imp()` – Find TX/RX ports
- `parse_section4()` – Extract geographic coordinates

### Display Functions
- `init_terminal_positioning()`
- `display_network_initial()`
- `update_imp_message()`
- `update_status_line()`
- `find_imp_by_number()`

### Message Processing
- `process_imp()`
- `process_regular()`
- `process_rfnm()`
- `process_host_dead()`
- `process_nop()`

---

## IMP Layer (`imp.c`)

- `imp_init_with_ports()`
- `imp_send_message()`
- `imp_receive_message()`
- `imp_shutdown()`
- `imp_host_ready()`

---

## Cleanup

- `cleanup()` – Signal handler
  - Clears host ready bit
  - Calls `imp_shutdown()`
  - Exits cleanly

---

## Important Implementation Details

### Scroll Prevention
- Status bar fixed at row 2
- No newline printing
- VT-100 clear screen
- `fflush(stdout)` after every update

### Terminal Compatibility
- Detects size via `ioctl(TIOCGWINSZ)`
- Defaults to 24 lines
- Requires ≥24 lines
- Map occupies bottom ~22 rows

---

## Performance

- Real-time incremental updates
- Full redraw every 10 seconds
- Non-blocking `select()`
- UDP for low latency

---

## Commented-Out Features

- Unix socket application interface
- Reserved for future WebSocket bridge

---

## Compilation & Usage

### Build
```sh
make
```

### Run
```sh
./ncc <imp_number> [config_file] 2>/dev/null
```

**Examples**
```sh
./ncc 32 2>/dev/null
./ncc 32 ../../arpanet-topology.conf 2>/dev/null
```

---

## Signal Handling

- SIGINT, SIGTERM, SIGQUIT
- Graceful cleanup
- Clears host ready bit before exit

---

## Files

- `ncc.c` – Main program (~2400 lines)
- `imp.c` – IMP protocol layer (263 lines)
- `imp.h` – IMP interface declarations
- `wire.h` – Wire protocol definitions
- `Makefile` – Build configuration

---

## Design Decisions

1. Status bar at top to prevent scroll
2. Real-time updates over batch redraw
3. VT-100 only for simplicity
4. No colors by design
5. UDP (matches IMP protocol)
6. Debug output to stderr

---

## Known Characteristics

- VT-100 terminal required
- Minimum 24-line terminal
- 72-column optimal width
- Probes every 10 seconds
- Supports up to 256 IMPs
- Coordinate space: **X(0–60), Y(0–20)**
