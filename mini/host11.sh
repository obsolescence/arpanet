#!/usr/bin/env bash

cd host11

set -euo pipefail

URL="https://obsolescence.dev/pidp10-sw/waits.zip"
ZIP="waits.zip"

REQUIRED_FILES=(
  "DISK.octal"
  "SYS000.ckd"
  "SYS001.ckd"
  "SYS002.ckd"
  "SYSTEM.DMP.49"
  "SYSTEM.DMP.K17"
)

missing=false
for f in "${REQUIRED_FILES[@]}"; do
    if [[ ! -f "$f" ]]; then
        echo "Missing: $f"
        missing=true
    fi
done

if $missing; then
    echo "Some files are missing — downloading WAITs archive…"

    if [[ ! -f "$ZIP" ]]; then
        curl -fL "$URL" -o "$ZIP"
    fi

    unzip -o "$ZIP"
else
    echo "All required files exist."
fi




#echo $PWD
#cp ./011/* .
echo not copying fresh system disks, assumed unnecessary for WAITS
echo in case of disk corruption, delete disk images and rerun script
echo
echo screen -dmS host11 ./pdp10-ka ./waits.ini
screen -dmS host11 ./pdp10-ka ./waits.ini
screen -dmS waitsconnect ./waitsconnect

#sleep 2
#screen -S host11 -p 0 -X clear
#sleep 2
#screen -S host11 -p 0 -X clear
#screen -S host11 -p 0 -X stuff $'R INFO\r'
cd ..

