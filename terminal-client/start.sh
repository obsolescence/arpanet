#!/bin/bash

# for recovery, use terminal_client_wrapper.sh instead of terminal_client.sh
if [ "$EUID" -eq 0 ]; then
    echo "You are root! Assuming VPS setup for terminal_client..."
    if screen -list | grep -q "term_client_screen"; then
        echo "Screen session 'term_client_screen' is already running."
    else
        echo "Starting screen session 'term_client_screen'..."
        screen -S term_client_screen bash -c "./terminal_client.sh"
        echo "Screen session 'term_client_screen' started."
fi
else
    echo "You are not root. Assuming local setup for terminal_client..."
    ./terminal_client.sh
fi

