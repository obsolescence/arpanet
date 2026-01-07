#!/bin/bash
# ==================================================
# ARPANET Simh Server
# ==================================================
# Manages up to 8 simh instances for terminal users
#
# Usage:
#   ./start.sh local    - Connect to local terminal-client only
#   ./start.sh vps      - Connect to VPS terminal-client only
#   ./start.sh both     - Connect to BOTH local and VPS (multi-connection)
#   ./start.sh          - Interactive mode (prompts for choice)
# ==================================================

cd "$(dirname "$0")"

echo "======================================"
echo "ARPANET Simh Server"
echo "======================================"
echo ""

# Activate virtual environment
if [ ! -d "venv" ]; then
    echo "Error: Virtual environment not found"
    echo "Please run: python3 -m venv venv && source venv/bin/activate && pip install websockets"
    exit 1
fi

source venv/bin/activate

# Determine mode
MODE="${1:-prompt}"

if [ "$MODE" = "prompt" ]; then
    echo "Select connection mode:"
    echo "  1) local - Connect to local terminal-client only"
    echo "  2) vps   - Connect to VPS terminal-client only"
    echo "  3) both  - Connect to BOTH local and VPS (multi-connection)"
    echo ""
    read -p "Choice (1/2/3): " choice
    case "$choice" in
        1) MODE="local" ;;
        2) MODE="vps" ;;
        3) MODE="both" ;;
        *) echo "Invalid choice"; exit 1 ;;
    esac
    echo ""
fi

# Set URL(s) based on mode
case "$MODE" in
    local)
        URLS="ws://localhost:8081"
        echo "Mode: Local Only"
        echo "  Connecting to: $URLS"
        ;;
    vps)
        URLS="wss://obsolescence.dev:8081"
        echo "Mode: VPS Only"
        echo "  Connecting to: $URLS"
        ;;
    both)
        URLS="ws://localhost:8081 wss://obsolescence.dev:8081"
        echo "Mode: Multi-Connection (Local + VPS)"
        echo "  Connecting to:"
        echo "    - ws://localhost:8081 (local)"
        echo "    - wss://obsolescence.dev:8081 (VPS)"
        ;;
    *)
        echo "Error: Invalid mode"
        echo "Usage: $0 [local|vps|both]"
        exit 1
        ;;
esac

# Check if do.sh exists (use absolute path)
SCRIPT_PATH="$(cd ../.. && pwd)/do.sh"
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "Error: do.sh not found at $SCRIPT_PATH"
    exit 1
fi

echo "  Script: $SCRIPT_PATH"
echo "  Max sessions: 8 (global limit)"
echo ""
echo "Press Ctrl+C to stop"
echo "======================================"
echo ""

# Pass URLs without quotes so they split into separate arguments
python3 simh_server.py $URLS "$SCRIPT_PATH"
