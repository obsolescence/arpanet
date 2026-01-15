#!/bin/bash
# start-home.sh - Start home bridge and frpc with clean shutdown

FRPC_PID=""
BRIDGE_PID=""

cleanup() {
    echo ""
    echo "=== Shutting down home components ==="

    if [ -n "$BRIDGE_PID" ]; then
        echo "Stopping home-bridge (PID: $BRIDGE_PID)..."
        kill $BRIDGE_PID 2>/dev/null
        wait $BRIDGE_PID 2>/dev/null
    fi

    if [ -n "$FRPC_PID" ]; then
        echo "Stopping frpc (PID: $FRPC_PID)..."
        kill $FRPC_PID 2>/dev/null
        wait $FRPC_PID 2>/dev/null
    fi

    echo "=== Home components stopped ==="
    exit 0
}

# Trap Ctrl-C and other termination signals
trap cleanup SIGINT SIGTERM

echo "=== Starting Home Components ==="
echo ""

# Start frpc
echo "Starting frpc..."
./frpc -c frpc-home.ini &
FRPC_PID=$!
echo "frpc started (PID: $FRPC_PID)"

# Wait a bit for frpc to initialize
sleep 2

# Start home-bridge
echo "Starting home-bridge..."
./home-bridge --verbose &
BRIDGE_PID=$!
echo "home-bridge started (PID: $BRIDGE_PID)"

echo ""
echo "=== Home Components Running ==="
echo "frpc PID:    $FRPC_PID"
echo "bridge PID:  $BRIDGE_PID"
echo ""
echo "Press Ctrl+C to stop all components"
echo ""
echo "Now attach IMP with: attach mi3 11312:localhost:11141"
echo "Or test with: python3 test-home.py"
echo ""

# Wait for processes (keeps script running)
wait
