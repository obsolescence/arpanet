#!/bin/bash
# Start simh PDP-1 simulator
# Wait for initial input (user presses Enter) before starting simh
# This ensures all simh output is captured when browser is connected


# SESSION_NUMBER is assumed to be 0..7
# export SESSION_NUMBER=5   # example

IMP_NUMBER=$(( 52 + (SESSION_NUMBER % 4) ))
HOST_NUMBER=$(( SESSION_NUMBER / 4 ))

export IMP_NUMBER
export HOST_NUMBER

echo "S-$SESSION_NUMBER I-$IMP_NUMBER H-$HOST_NUMBER"

# Silently wait for user to press Enter
read -r dummy

#./telnet.sh

#cd ./mini
#./dotelnet.sh $IMP_NUMBER $HOST_NUMBER

#cd ./mini/src/ncc
#./ncc 6 2>/dev/null

echo -------------------------------------
echo
echo Arpanet server is offline today :-(
echo
echo -------------------------------------

