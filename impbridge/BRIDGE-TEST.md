# Bridge Testing with Python Scripts

## Purpose

Test if the bridges work correctly using Python programs (unconnected sockets) instead of IMPs (connected sockets). This isolates whether the problem is in the bridges or in handling connected sockets.

## Test Setup

### On Home
1. **Terminal 1:** Start frpc
   ```bash
   ./frpc -c frpc-home.ini
   ```

2. **Terminal 2:** Start home-bridge
   ```bash
   ./home-bridge --verbose
   ```

3. **Terminal 3:** Run Python test
   ```bash
   python3 test-home.py
   ```

### On Guest
1. **Terminal 1:** Start frpc
   ```bash
   ./frpc -c frpc-guest.ini
   ```

2. **Terminal 2:** Start guest-bridge
   ```bash
   ./guest-bridge --verbose
   ```

3. **Terminal 3:** Run Python test
   ```bash
   python3 test-guest.py
   ```

### On VPS
```bash
./frps -c frps.ini
```

## Expected Behavior

**If bridges work correctly:**

**Home output:**
```
[HOME-TEST 15:30:00] → SENT: 'hello from home #1'
[HOME-TEST 15:30:01] ✓ RECEIVED: 'hi from guest #1' from ('127.0.0.1', XXXXX)
[HOME-TEST 15:30:03] → SENT: 'hello from home #2'
[HOME-TEST 15:30:04] ✓ RECEIVED: 'hi from guest #2' from ('127.0.0.1', XXXXX)
```

**Guest output:**
```
[GUEST-TEST 15:30:00] ✓ RECEIVED: 'hello from home #1' from ('127.0.0.1', XXXXX)
[GUEST-TEST 15:30:00] → SENT: 'hi from guest #1'
[GUEST-TEST 15:30:03] ✓ RECEIVED: 'hello from home #2' from ('127.0.0.1', XXXXX)
[GUEST-TEST 15:30:03] → SENT: 'hi from guest #2'
```

**Home-bridge logs (verbose):**
```
[IMP→VPS] 20 bytes
[VPS→IMP] 18 bytes (sourceport=11141)
```

**Guest-bridge logs (verbose):**
```
[VPS→IMP] 20 bytes (sourceport=11199)
[IMP→VPS] 18 bytes
```

## What This Tests

✓ Bridge programs compile and run
✓ Bridges can receive from "IMP" ports
✓ Bridges can send to VPS
✓ Bridges can receive from frpc
✓ Bridges can send to "IMP" ports
✓ frp relay works (VPS routing)
✓ Basic packet forwarding chain

## What This Does NOT Test

✗ Connected socket source port filtering (Python uses unconnected sockets)
✗ Actual IMP protocol/behavior

## Troubleshooting

### Python receives nothing

**Check bridge is running:**
```bash
ps aux | grep bridge
```

**Check bridge logs (verbose mode):**
- Should see `[IMP→VPS]` when Python sends
- Should see `[VPS→IMP]` when packets arrive from VPS

**Check frpc is connected:**
```bash
# frpc log should show "start proxy success"
```

### Bridge shows errors

**"bind: Address already in use"**
```bash
# Find what's using the port
sudo lsof -i :11141  # or whichever port fails
```

**"sendto: Network unreachable"**
- Check VPS IP is correct (50.6.201.221)
- Verify network connectivity: `ping 50.6.201.221`

### frpc not connecting

**Check VPS is running frps:**
```bash
# On VPS
ps aux | grep frps
```

**Check firewall:**
- VPS must allow ports 6001, 6002, 11999

## Interpreting Results

### If Python test works ✓

**The bridges are functioning correctly!**

The problem is specifically with connected sockets. This means:
- The bridge source port binding might not be working as expected
- OR there's another issue with connected socket filtering

**Next step:** Verify source port with tcpdump while Python test runs:
```bash
# On guest
sudo tcpdump -i lo -n udp port 11198 -v
```

Should show source port 11199 (not random).

### If Python test fails ✗

**The bridges or frp setup have issues.**

Possibilities:
- Bridge logic error
- frpc not forwarding correctly
- VPS routing problem
- Network connectivity issue

**Next step:** Test each hop individually (see below).

## Individual Component Testing

### Test 1: Bridge Send Path Only

**On home, send test packet:**
```bash
echo "test" | nc -u 127.0.0.1 11141
```

**Check home-bridge log:** Should show `[IMP→VPS]`

**On VPS, capture:**
```bash
sudo tcpdump -i any -n udp port 6002
```

Should see packet arrive at VPS.

### Test 2: Bridge Receive Path Only

**On guest, listen:**
```bash
nc -u -l 11198
```

**On home, inject packet to frpc:**
```bash
echo "test" | nc -u 127.0.0.1 31312
```

**Check:** Guest bridge should forward to 11198, nc should receive it.

### Test 3: Check Source Port

**On guest:**
```bash
# Terminal 1: tcpdump
sudo tcpdump -i lo -n udp port 11198 -v

# Terminal 2: guest-bridge running

# Terminal 3: Send test packet to bridge
echo "test" | nc -u 127.0.0.1 31141
```

**Check tcpdump output:** Should show source port 11199.

If NOT 11199, the bridge's source port binding isn't working!
