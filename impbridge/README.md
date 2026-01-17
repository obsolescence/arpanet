# C Bridge Test Setup for IMP-to-IMP Communication

## Overview

This directory contains everything needed to connect a guest IMP to a home IMP through a VPS using frp and custom UDP bridges.

## Architecture

```
Home IMP                                                    Guest IMP
--------                                                    ---------
Receives: localhost:11312                                   Receives: localhost:11198
Sends to: localhost:11141                                   Sends to: localhost:11199
        ↓                                                           ↓
    home-bridge                                             guest-bridge
        ↓                                                           ↓
    frpc (home)                                             frpc (guest)
        ↓                                                           ↓
        └──────────────────> VPS (frps) <──────────────────────────┘
                         50.6.201.221
                         ports 6001/6002
```

## Files

### Source Code
- `guest-bridge.c` - Guest bridge program
- `home-bridge.c` - Home bridge program
- `Makefile` - Build both programs

### Configuration (frp)
- `frps.ini` - VPS server config (copy to VPS)
- `frpc-home.ini` - Home client config (use on home)
- `frpc-guest.ini` - Guest client config (use on guest)

### Scripts
- `start-vps.sh` - Start frps on VPS
- `start-home.sh` - Start home bridge + frpc
- `start-guest.sh` - Start guest bridge + frpc

### Documentation
- `README.md` - This file
- `SETUP.md` - Detailed setup instructions
- `IMP-CONFIG.md` - IMP attach commands

## Quick Start

### 1. Build the bridges
```bash
make
```

### 2. On VPS
```bash
./start-vps.sh
```

### 3. On Home Server
```bash
./start-home.sh
```

### 4. On Guest Machine
```bash
./start-guest.sh
```

### 5. Configure IMPs

**Home IMP:**
```
attach mi3 11312:localhost:11141
```

**Guest IMP:**
```
attach mi1 11198:localhost:11199
```

## Port Reference

| Machine | Port | Purpose |
|---------|------|---------|
| **VPS** | 11999 | frp control port |
| **VPS** | 6001 | Guest→Home relay |
| **VPS** | 6002 | Home→Guest relay |
| **Home** | 11312 | IMP receive |
| **Home** | 11141 | IMP send (bridge receives) |
| **Home** | 31312 | frpc forwards here (bridge receives) |
| **Guest** | 11198 | IMP receive |
| **Guest** | 11199 | IMP send (bridge receives) |
| **Guest** | 31141 | frpc forwards here (bridge receives) |

## Testing

See `SETUP.md` for detailed testing procedures.
