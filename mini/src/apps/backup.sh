#!/bin/bash

timestamp=$(date +"%Y%m%d-%H%M%S")
zipfile="${timestamp}-backup.zip"

zip -r "$zipfile" \
    $(find . -maxdepth 1 -type f \( -name "*.c" -o -name "*.h" \)) \
    $(find ./ncpd -type f \( -name "*.c" -o -name "*.h" \))

echo "Created $zipfile"

