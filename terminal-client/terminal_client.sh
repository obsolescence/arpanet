#!/bin/bash
# ==================================================
# ARPANET Terminal Client
# ==================================================
# Accepts browser connections and proxies to simh-server
#
# Auto-detects environment:
#   - On VPS: Uses SSL with certificates
#   - On laptop: Plain WebSocket (no SSL)
#
# Usage:
#   ./start.sh
# ==================================================
#if you need to install websockets under venv, do this on the server"
#python3 -m venv venv
#  source venv/bin/activate
#  pip install websockets


# determine whether we're running on VPS (as root) or not
if [ "$EUID" -eq 0 ]; then
    echo "You are root! Full path name for SSL certs..."
    VPATH="/home/<user>"
else
    echo "You are not root. ~ path name for SSL certs..."
    VPATH="~"
fi



cd "$(dirname "$0")"

echo "======================================"
echo "ARPANET Terminal Client"
echo "======================================"
echo ""

# Activate virtual environment
if [ ! -d "venv" ]; then
    echo "Error: Virtual environment not found"
    echo "Please run: python3 -m venv venv && source venv/bin/activate && pip install websockets"
    exit 1
fi

source venv/bin/activate

# Auto-detect SSL certificates (VPS vs laptop)
CERT=$(ls -t ${VPATH}/ssl/certs/obsolescence_dev*.crt 2>/dev/null | grep -v "^_wildcard" | head -1)

if [ -n "$CERT" ]; then
    # Running on VPS with SSL
    CERT_BASENAME=$(basename "$CERT")
    CERT_ID=$(echo "$CERT_BASENAME" | sed 's/obsolescence_dev_\([^_]*_[^_]*\).*/\1/')
    KEY=$(ls ${VPATH}/ssl/keys/${CERT_ID}*.key 2>/dev/null | head -1)

    if [ -n "$KEY" ]; then
        echo "Mode: VPS (with SSL/TLS)"
        echo "  Browsers:    wss://0.0.0.0:8080"
        echo "  Simh-server: wss://0.0.0.0:8081"
        echo ""
        echo "Certificate: $CERT"
        echo "Key: $KEY"
        echo ""
        echo "Press Ctrl+C to stop"
        echo "======================================"
        echo ""
        python3 terminal_client.py "$CERT" "$KEY"
    else
        echo "Error: Certificate found but matching key not found"
        echo "Certificate: $CERT"
        exit 1
    fi
else
    # Running on laptop without SSL
    echo "Mode: Local (without SSL)"
    echo "  Browsers:    ws://localhost:8080"
    echo "  Simh-server: ws://localhost:8081"
    echo ""
    echo "Press Ctrl+C to stop"
    echo "======================================"
    echo ""
    python3 terminal_client.py
fi
