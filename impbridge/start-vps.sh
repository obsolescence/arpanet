#!/bin/bash
# start-vps.sh - Start frps on VPS

echo "Starting frps on VPS..."
echo "Control port: 11999"
echo "Guest→Home:   6001"
echo "Home→Guest:   6002"
echo ""

./frps -c frps.ini
