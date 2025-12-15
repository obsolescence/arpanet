while true; do
    echo "Starting WSS client at $(date)"
    ./terminal_client.sh
    echo "Client crashed / exited ($?). Restarting in 5 seconds..."
    sleep 5
done
