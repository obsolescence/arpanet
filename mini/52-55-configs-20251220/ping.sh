#!/bin/bash
#
# ARPANET Ping Utility
# Usage:
#   ./ping.sh <source_host>              - Ping all hosts from source
#   ./ping.sh <source_host> <dest_host>  - Ping specific destination from source
#

if [ -z "$1" ]; then
  echo "Usage: $0 <source_host> [destination_host]"
  echo ""
  echo "Examples:"
  echo "  $0 2       # Ping all hosts from host 2"
  echo "  $0 2 32    # Ping host 32 from host 2"
  exit 1
fi

# Format source host with zero-padding (at least 2 digits)
SOURCE=$(printf "%02d" $1)

# Extract list of host 0 from each IMP (only host index 0)
if [ -f "./arpanet" ]; then
  HOSTS=($(sed -n '/declare -a NCPS=/,/^)/p' ./arpanet | grep -oE '"[^"]+' | awk -F: '$2 == "0" {print $3}'))
else
  echo "Error: ./arpanet script not found"
  exit 1
fi

if [ ${#HOSTS[@]} -eq 0 ]; then
  echo "Error: Could not extract host list from arpanet script"
  exit 1
fi

# Check if source host exists
if ! printf '%s\n' "${HOSTS[@]}" | grep -qx "$SOURCE"; then
  echo "Error: Source host $SOURCE not found in network"
  echo "Available hosts: ${HOSTS[*]}"
  exit 1
fi

if [ -z "$2" ]; then
  # Ping all hosts except self
  echo "Pinging all hosts from host $SOURCE..."
  echo ""

  for host in "${HOSTS[@]}"; do
    if [ "$host" != "$SOURCE" ]; then
      echo "ping $SOURCE->$host:"
      NCP=ncp$SOURCE ./ncp-ping -c1 $host
      echo
    fi
  done
else
  # Ping specific destination
  DEST=$(printf "%02d" $2)

  # Check if destination host exists
  if ! printf '%s\n' "${HOSTS[@]}" | grep -qx "$DEST"; then
    echo "Error: Destination host $DEST not found in network"
    echo "Available hosts: ${HOSTS[*]}"
    exit 1
  fi

  if [ "$SOURCE" == "$DEST" ]; then
    echo "Error: Cannot ping self (source and destination are the same)"
    exit 1
  fi

  echo "ping $SOURCE->$DEST:"
  NCP=ncp$SOURCE ./ncp-ping -c1 $DEST
  echo
fi
