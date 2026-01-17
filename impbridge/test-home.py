#!/usr/bin/env python3
"""
test-home.py - Test home bridge
Mimics home IMP behavior without connected sockets

<<<<<<< HEAD
Sends to: localhost:11162 (where home-bridge expects IMP to send)
Receives on: localhost:11331 (where home-bridge sends to IMP)
=======
Sends to: localhost:11141 (where home-bridge expects IMP to send)
Receives on: localhost:11312 (where home-bridge sends to IMP)
>>>>>>> 631e93f342c01900b3bdfd3f222396d8e039ae6e
"""

import socket
import time
import threading

# Match home IMP ports
SEND_PORT = 11162      # home-bridge receives from IMP here
RECEIVE_PORT = 11331   # home-bridge sends to IMP here
<<<<<<< HEAD
SEND_INTERVAL = 1      # seconds
=======
SEND_INTERVAL = 3      # seconds
>>>>>>> 631e93f342c01900b3bdfd3f222396d8e039ae6e

def receiver():
    """Listen for incoming UDP messages"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', RECEIVE_PORT))
    print(f"[HOME-TEST] Listening on localhost:{RECEIVE_PORT}")

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            message = data.decode('utf-8')
            timestamp = time.strftime('%H:%M:%S')
            print(f"[HOME-TEST {timestamp}] ✓ RECEIVED: '{message}' from {addr}")
        except Exception as e:
            print(f"[HOME-TEST] Receive error: {e}")

def sender():
    """Send messages periodically"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"[HOME-TEST] Sending to localhost:{SEND_PORT} every {SEND_INTERVAL}s")

    counter = 0
    while True:
        try:
            counter += 1
            message = f"hello from home #{counter}"
            sock.sendto(message.encode('utf-8'), ('127.0.0.1', SEND_PORT))
            timestamp = time.strftime('%H:%M:%S')
            print(f"[HOME-TEST {timestamp}] → SENT: '{message}'")
        except Exception as e:
            print(f"[HOME-TEST] Send error: {e}")

        time.sleep(SEND_INTERVAL)

if __name__ == '__main__':
    print("=" * 60)
    print("HOME BRIDGE TEST PROGRAM")
    print("=" * 60)
    print(f"Tests home-bridge by sending/receiving on IMP ports")
    print(f"Send to:     localhost:{SEND_PORT}")
    print(f"Receive on:  localhost:{RECEIVE_PORT}")
    print("=" * 60)
    print()

    # Start receiver in background thread
    receiver_thread = threading.Thread(target=receiver, daemon=True)
    receiver_thread.start()

    # Small delay to let receiver start
    time.sleep(0.5)

    # Run sender in main thread
    try:
        sender()
    except KeyboardInterrupt:
        print("\n[HOME-TEST] Shutting down...")
