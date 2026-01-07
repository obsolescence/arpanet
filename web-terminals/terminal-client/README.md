# ARPANET Terminal Client

Accepts browser connections and proxies them to simh-server.
Runs on VPS (with SSL) or laptop (without SSL) - auto-detected.

## What It Does

- Accepts browser WebSocket connections on port 8080
- Accepts simh-server connection on port 8081
- Multiplexes multiple browser sessions through one simh-server connection
- Routes messages between browsers and their simh instances
- Handles unlimited browser connections (limited by simh-server to 8)

## Architecture

```
Browser 1 → terminal-client:8080 ↘
Browser 2 → terminal-client:8080  → terminal-client:8081 ← simh-server (laptop)
Browser N → terminal-client:8080 ↗
```

## Quick Start

### Local Testing (on laptop)

```bash
cd ~/Documents/x4/arpa/terminal-client
./start.sh
```

Output:
```
Mode: Local (without SSL)
  Browsers:    ws://localhost:8080
  Simh-server: ws://localhost:8081
```

### VPS Production

```bash
cd ~/arpa/terminal-client
./start.sh
```

Output:
```
Mode: VPS (with SSL/TLS)
  Browsers:    wss://0.0.0.0:8080
  Simh-server: wss://0.0.0.0:8081
Certificate: /home/user/ssl/certs/...
```

## Auto-Detection

The startup script automatically detects the environment:

**VPS Detected if:**
- SSL certificates found in `~/ssl/certs/obsolescence_dev*.crt`
- Matching key found in `~/ssl/keys/`
- Uses WSS (encrypted WebSockets)

**Laptop Detected if:**
- No SSL certificates found
- Uses WS (plain WebSockets)

## Ports

- **8080** - Browser connections
- **8081** - Simh-server connection

## SSL/TLS

### VPS Mode (Automatic)
- Uses Bluehost AutoSSL certificates
- Certificates auto-discovered from `~/ssl/certs/`
- Keys auto-discovered from `~/ssl/keys/`
- Browsers connect via `wss://obsolescence.dev:8080`
- Simh-server connects via `wss://obsolescence.dev:8081`

### Local Mode (Automatic)
- No SSL certificates needed
- Browsers connect via `ws://localhost:8080`
- Simh-server connects via `ws://localhost:8081`

## Message Protocol

### Browser ↔ Terminal-Client
```json
{"type": "input", "data": "ls\n"}
{"type": "output", "data": "file1\nfile2\n"}
{"type": "resize", "cols": 80, "rows": 24}
{"type": "setBaudRate", "baudRate": 1200}
```

### Terminal-Client ↔ Simh-Server (Multiplexed)
```json
{"session": "session_123", "type": "new_session"}
{"session": "session_123", "type": "input", "data": "ls\n"}
{"session": "session_123", "type": "output", "data": "file1\n"}
{"session": "session_123", "type": "close_session"}
```

## Requirements

- Python 3.7+
- websockets library (`pip install websockets`)
- For VPS: SSL certificates in `~/ssl/`

## Files

- `terminal_client.py` - Main server code
- `start.sh` - Auto-detecting startup script
- `venv/` - Virtual environment with dependencies

## Deployment to VPS

```bash
# On laptop
cd ~/Documents/x4/arpa/terminal-client
scp -r . user@vps:~/arpa/terminal-client/

# On VPS
cd ~/arpa/terminal-client
python3 -m venv venv
source venv/bin/activate
pip install websockets
./start.sh  # Auto-detects VPS mode

# Set up as systemd service (optional)
sudo systemctl enable arpanet-terminal-client
sudo systemctl start arpanet-terminal-client
```

## Troubleshooting

**"Virtual environment not found"**
```bash
python3 -m venv venv
source venv/bin/activate
pip install websockets
```

**"Certificate found but matching key not found"**
- Check certificate ID in filename
- Verify matching key exists in `~/ssl/keys/`
- Key filename should contain same ID as certificate

**"Address already in use"**
- Another process is using port 8080 or 8081
- Check: `netstat -tulpn | grep 808`
- Kill existing process or use different ports

**Browsers can't connect**
- Check firewall allows port 8080
- Verify terminal-client is running
- Check browser console for WebSocket errors

**Simh-server can't connect**
- Check firewall allows port 8081
- Verify terminal-client is running
- Check simh-server logs for connection errors

## Development

To change ports, edit `terminal_client.py`:
```python
server = TerminalClient(browser_port=8080, simh_port=8081, ...)
```

To add debugging:
```python
logging.basicConfig(level=logging.DEBUG, ...)
```
