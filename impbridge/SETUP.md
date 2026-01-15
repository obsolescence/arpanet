# Complete Setup Instructions

## Prerequisites

- **VPS**: frp server binary (`frps`)
- **Home**: frp client binary (`frpc`), C compiler (gcc)
- **Guest**: frp client binary (`frpc`), C compiler (gcc)

## Build

On both home and guest machines:
```bash
cd cbridge
make
```

This creates `home-bridge` and `guest-bridge` executables.

## Startup Sequence

### 1. VPS (50.6.201.221)

```bash
# Copy frps.ini to VPS
# Start frp server
./frps -c frps.ini
```

**Expected output:**
```
[I] [service.go:XXX] frps started successfully
```

**Verify firewall allows:**
- Port 11999 (frp control)
- Port 6001 (guest→home relay)
- Port 6002 (home→guest relay)

### 2. Home Server

**Terminal 1 - Start frpc:**
```bash
./frpc -c frpc-home.ini
```

**Expected:** "start proxy success" for `home_receive`

**Terminal 2 - Start bridge:**
```bash
./home-bridge --verbose
```

**Expected output:**
```
=== Home Bridge Starting ===
IMP receive: 127.0.0.1:11312
IMP send:    127.0.0.1:11141
VPS:         50.6.201.221:6002
frpc→bridge: 127.0.0.1:31312
============================

[INIT] Recv-from-IMP socket bound to 127.0.0.1:11141
[INIT] Recv-from-frpc socket bound to 127.0.0.1:31312
[INIT] Send-to-IMP socket bound to source port 11141
[INIT] Send-to-VPS socket created

[READY] Bridge running, press Ctrl+C to stop
```

**Terminal 3 - Start IMP:**
```
# In SIMH
attach mi3 11312:localhost:11141
```

### 3. Guest Machine

**Terminal 1 - Start frpc:**
```bash
./frpc -c frpc-guest.ini
```

**Expected:** "start proxy success" for `guest_receive`

**Terminal 2 - Start bridge:**
```bash
./guest-bridge --verbose
```

**Expected output:**
```
=== Guest Bridge Starting ===
IMP receive: 127.0.0.1:11198
IMP send:    127.0.0.1:11199
VPS:         50.6.201.221:6001
frpc→bridge: 127.0.0.1:31141
=============================

[INIT] Recv-from-IMP socket bound to 127.0.0.1:11199
[INIT] Recv-from-frpc socket bound to 127.0.0.1:31141
[INIT] Send-to-IMP socket bound to source port 11199
[INIT] Send-to-VPS socket created

[READY] Bridge running, press Ctrl+C to stop
```

**Terminal 3 - Start IMP:**
```
# In SIMH
attach mi1 11198:localhost:11199
```

## Testing

### Test 1: Bridge Startup

Verify all components are running:

**On Home:**
```bash
ps aux | grep -E 'frpc|home-bridge'
sudo lsof -i :11141,11312,31312
```

**On Guest:**
```bash
ps aux | grep -E 'frpc|guest-bridge'
sudo lsof -i :11198,11199,31141
```

### Test 2: Packet Flow

With `--verbose` flag, bridges log all packets:

**When home IMP sends:**
```
[IMP→VPS] 146 bytes
```

**When guest bridge receives from VPS:**
```
[VPS→IMP] 146 bytes (sourceport=11199)
```

### Test 3: Source Port Verification

**On Guest:**
```bash
sudo tcpdump -i lo -n udp port 11198 -v
```

**Expected:** Packets with source port 11199 (not random ports)

### Test 4: IMP Connection

If everything works, IMPs should:
- Exchange handshake packets
- Show "line up" status
- Establish IMP-to-IMP communication

## Troubleshooting

### Bridge won't start - "bind: Address already in use"

**Cause:** Port conflict

**Solution:**
```bash
# Find what's using the port
sudo lsof -i :11141  # or whichever port fails

# Kill conflicting process or change port
```

### No packets flowing

**Check frpc connection:**
```bash
# frpc should show "start proxy success"
# If not, check VPS is reachable
ping 50.6.201.221
```

**Check bridge is running:**
```bash
ps aux | grep bridge
```

### IMPs don't connect

**Verify IMP attach command:**
```bash
# Check lsof output matches expected
sudo lsof -i :11312 -n  # Home
sudo lsof -i :11198 -n  # Guest
```

**Check bridge logs (--verbose mode):**
- Should see [IMP→VPS] when IMP sends
- Should see [VPS→IMP] when packets arrive

**Verify source port:**
```bash
sudo tcpdump -i lo -n udp port 11198
# Should show sourceport 11199
```

## Port Reference Table

| Machine | Port | Component | Direction | Purpose |
|---------|------|-----------|-----------|---------|
| VPS | 11999 | frps | - | Control port |
| VPS | 6001 | frps | IN | Guest→Home relay |
| VPS | 6002 | frps | IN | Home→Guest relay |
| Home | 11312 | IMP | IN | Receives packets |
| Home | 11141 | IMP/Bridge | OUT/IN | IMP sends / Bridge receives |
| Home | 31312 | Bridge | IN | frpc forwards here |
| Guest | 11198 | IMP | IN | Receives packets |
| Guest | 11199 | IMP/Bridge | OUT/IN | IMP sends / Bridge receives |
| Guest | 31141 | Bridge | IN | frpc forwards here |

## Success Indicators

✓ frps running on VPS
✓ frpc connected on both sides
✓ Bridges showing [READY]
✓ IMPs attached with correct ports
✓ Bridge logs show packet flow
✓ tcpdump shows correct source ports
✓ IMPs establish connection
