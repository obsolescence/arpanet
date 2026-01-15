# IMP Configuration Commands

## Home IMP

In SIMH console:
```
attach mi3 11312:localhost:11141
```

**Explanation:**
- `11312` - Port where IMP receives packets
- `localhost` - Connect to localhost (not VPS)
- `11141` - Port where IMP sends packets

**Result:**
- IMP listens on 127.0.0.1:11312
- IMP connected to 127.0.0.1:11141
- Only accepts packets from source 127.0.0.1:11141

## Guest IMP

In SIMH console:
```
attach mi1 11198:localhost:11199
```

**Explanation:**
- `11198` - Port where IMP receives packets
- `localhost` - Connect to localhost (not VPS)
- `11199` - Port where IMP sends packets

**Result:**
- IMP listens on 127.0.0.1:11198
- IMP connected to 127.0.0.1:11199
- Only accepts packets from source 127.0.0.1:11199

## Verification

After attaching, verify with lsof:

**Home:**
```bash
sudo lsof -i :11312 -n
# Should show: UDP 127.0.0.1:11312->127.0.0.1:11141
```

**Guest:**
```bash
sudo lsof -i :11198 -n
# Should show: UDP 127.0.0.1:11198->127.0.0.1:11199
```
