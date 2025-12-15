# ARPANET Simh Server

Manages up to 8 concurrent simh-pdp1 instances for terminal users.

## What It Does

- Connects to terminal-client (which serves browsers)
- Spawns a separate simh-pdp1 instance for each user session
- Routes terminal I/O between browsers and their simh instances
- Handles up to 8 concurrent users (9th user gets "busy" message)
- Cleans up simh processes when users disconnect

## Architecture

```
terminal-client (VPS or laptop)
         ↓ multiplexed sessions
simh-server (always on laptop)
         ↓
    simh-pdp1 instance 1
    simh-pdp1 instance 2
    simh-pdp1 instance N (max 8)
```

## Quick Start

### Local Testing (everything on laptop)

```bash
# Terminal 1: Start terminal-client
cd ~/Documents/x4/arpa/terminal-client
./start.sh

# Terminal 2: Start simh-server
cd ~/Documents/x4/arpa/simh-server
./start.sh local

# Browser: Open local file
file:///home/x/Documents/x4/arpa/arpanet_terminal2.html
```

### VPS Production (terminal-client on VPS, simh-server on laptop)

```bash
# On laptop
cd ~/Documents/x4/arpa/simh-server
./start.sh vps

# On VPS (terminal-client should already be running)
# Browsers access: https://obsolescence.dev/arpanet_terminal2.html
```

## Usage

```bash
./start.sh [local|vps]
```

**Modes:**
- `local` - Connect to terminal-client on localhost (for testing)
- `vps` - Connect to terminal-client on VPS (for production)
- No argument - Interactive prompt

## Connection URLs

- **Local mode:** `ws://localhost:8081` (no encryption)
- **VPS mode:** `wss://obsolescence.dev:8081` (encrypted)

## Session Management

- **Maximum 8 concurrent sessions**
- Each session gets independent simh instance
- 9th connection receives "busy" message
- Sessions automatically cleaned up on disconnect

## Requirements

- Python 3.7+
- websockets library (`pip install websockets`)
- simh-pdp1 installed and in PATH
- do.sh script in parent directory

## Files

- `simh_server.py` - Main server code
- `start.sh` - Simple startup script
- `venv/` - Virtual environment with dependencies

## Troubleshooting

**"Virtual environment not found"**
```bash
python3 -m venv venv
source venv/bin/activate
pip install websockets
```

**"Script not found: ../do.sh"**
- Ensure do.sh exists in the parent directory
- Check that it's executable: `chmod +x ../do.sh`

**"Connection failed"**
- Check that terminal-client is running
- Verify the URL (ws://localhost:8081 for local, wss://vps:8081 for VPS)
- Check firewall settings

**Sessions not cleaning up**
- Simh processes should terminate automatically
- If stuck: `pkill simh-pdp1`

## Development

To modify session limit, edit `simh_server.py`:
```python
self.max_sessions = 8  # Change this value
```

To change baud rate defaults, edit:
```python
self.baud_rate = 9600  # Default baud rate
```
