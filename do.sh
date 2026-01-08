#!/bin/bash
set -euo pipefail

# SESSION_NUMBER: argument, if given, overrides environment SESSION_NUMBER
if [[ -n "${1-}" ]]; then
    SESSION_NUMBER="$1"
elif [[ -n "${SESSION_NUMBER-}" ]]; then
    :  # use existing environment variable
else
    echo "SESSION_NUMBER not set (argument or environment)" >&2
    exit 1
fi
# Define the array of IMP_NUMBER and HOST_NUMBER pairs
imp_host_pairs=(
    "12 0"
    "31 0"
    "4 0"
    "10 0"
    "10 1"
    "13 0"
    "14 0"
    "14 1"
)

# Bounds check (important!)
if (( SESSION_NUMBER < 0 || SESSION_NUMBER >= ${#imp_host_pairs[@]} )); then
    echo "Invalid SESSION_NUMBER: $SESSION_NUMBER" >&2
    exit 1
fi

# Get the pair
pair="${imp_host_pairs[$SESSION_NUMBER]}"

# Split into variables
read -r IMP_NUMBER HOST_NUMBER <<< "$pair"

#echo "S-$SESSION_NUMBER I-$IMP_NUMBER H-$HOST_NUMBER"


# ==================================================
#
# Simulate TIP behaviour: wait for a @L <host> line,
# then use that host number
#
# ==================================================

while IFS= read -r line; do
    # Remove possible CR (for safety if CRLF sneaks in)
    line=${line%$'\r'}

#    if [[ $line =~ ^@L[[:space:]]*([0-9]+) ]]; then
if [[ $line =~ ^@[lLoO][[:space:]]*([0-9]+) ]]; then
        DEST="${BASH_REMATCH[1]}"
	break
    fi
done
#echo "---> connect $DEST"

# DEST: either 2nd argument or prompt
#DEST="${2:-}"
#if [[ -z "$DEST" ]]; then
#    read -r DEST
#fi

cd ./mini
./dotelnet.sh "$IMP_NUMBER" "$HOST_NUMBER" "$DEST"

