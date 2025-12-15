#!/bin/bash

# This script creates a compressed zip archive of key arpanet project files and directories.

# 1. Generate a timestamp in YYYYMMDD-HHMMSS format
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
FILENAME="${TIMESTAMP}-arpabackup.zip"

echo "Creating backup archive: ${FILENAME}"

# 2. Create the zip archive.
# The '-r' flag recursively includes directory contents.
# The '-x' flag excludes files and directories matching the given pattern.
# File paths are listed directly.
zip -r "${FILENAME}" \
    arpanet_*.html \
    *.sh \
    arpa/*.json \
    arpa/arpanet-node-x-template.html \
    ./terminal-client \
    ./simh-server \
    ./mini \
    ./arpa/assets \
    -x "*venv/*" \
       "mini/rp*" \
       "mini/dsk*" \
       "mini/*.zip"

# 3. Verify the archive was created and provide feedback
if [ -f "${FILENAME}" ]; then
    echo ""
    echo "Successfully created backup archive:"
    ls -lh "${FILENAME}"
else
    echo "Error: Backup file could not be created."
    exit 1
fi
