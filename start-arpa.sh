#!/usr/bin/env bash

# Helper function to ask a yes/no question
ask_yes_no() {
    local prompt="$1"
    read -rp "$prompt [y/N]: " answer
    [[ "$answer" == "y" || "$answer" == "Y" ]]
}

echo "=== ARPANET Startup Script ==="
echo


# ------------------------------------------------------------
# 0. Dependency checks
# ------------------------------------------------------------
echo "Section 0: Checking dependencies..."
missing=0

# Check Node.js
if command -v node >/dev/null 2>&1; then
    echo "  Node.js found: $(node --version)"
else
    echo "  Node.js NOT found (required for WebSocket client)"
    missing=1
fi

# Check Python 3
if command -v python3 >/dev/null 2>&1; then
    echo "  Python 3 found: $(python3 --version)"
else
    echo "  Python 3 NOT found (required for WebSocket server)"
    missing=1
fi

# Check Python websockets module
if python3 - <<'EOF' >/dev/null 2>&1
import websockets
EOF
then
    echo "  Python websockets module found"
else
    echo "  Python websockets module NOT found (pip install websockets)"
    missing=1
fi

if [[ $missing -ne 0 ]]; then
    echo
    echo "WARNING: One or more dependencies are missing."
    echo "Some components will fail to start,"
    echo "unless you maybe have your python websockets in a venv?"
    echo "otherwise, abort the script and fix."
fi

echo

# 1. Restore SIMH system disks
if ask_yes_no "Restore simh system disks -required for first start?"; then
    echo "Restoring SIMH system disks..."
    echo cp ./mini/its-disk-images-clean/* ./mini/
    cp ./mini/its-disk-images-clean/* ./mini/
fi
echo

# 2. Start Arpanet (PDP-10 / SIMH)
if ask_yes_no "Start up the Arpanet?"; then
    echo "Starting PDP-10 in screen session 'pdp10'..."
    cd mini
    echo screen -dmS pdp10 ./start
    screen -dmS pdp10 ./start
    cd ..
fi
echo

# 3. Start local WebSocket server
if ask_yes_no "Start up local WebSocket server?"; then
    echo "Starting WebSocket server in screen session 'simh-server'..."
    echo screen -dmS simh-server ./simh_server/start.sh
    cd ./simh-server
    screen -dmS simh-server ./start.sh both
    cd ..
fi
echo

# 4. Start local WebSocket client
if ask_yes_no "Start up local WebSocket client?"; then
    echo "Starting WebSocket client in screen session 'terminal-client'..."
    echo screen -dmS terminal-client ./terminal-client/start.sh
    cd ./terminal-client
    screen -dmS terminal-client ./start.sh
    cd ..
fi
echo

echo "Done."
echo "Load \(File → Open\) ./arpanet_home in your browser to start."
echo "Use stop-arpa.sh to shut down in an orderly fashion."
echo
echo "Use screen -ls to monitor the various components"
echo

