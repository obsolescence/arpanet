#!/usr/bin/env python3
"""
WebSocket-to-TCP Bridge for VT-52 Simulator

Allows VT-52 simulator to connect through the existing ARPANET terminal infrastructure.
Acts as a transparent bridge between TCP (VT-52) and WebSocket (terminal-client).

Architecture:
  VT-52 simulator → TCP (localhost:10018) → ws_bridge → WebSocket → terminal-client (VPS) → simh-server → simh

Usage:
  python3 ws_bridge.py                                    # Default: localhost:10018
  python3 ws_bridge.py <tcp_port>                         # Custom TCP port
  python3 ws_bridge.py <tcp_port> <ws_url>                # Custom port and WebSocket URL

Examples:
  python3 ws_bridge.py                                    # Listen on 10018, connect to ws://localhost:8080
  python3 ws_bridge.py 10018 ws://localhost:8080          # Explicit local mode
  python3 ws_bridge.py 10018 wss://obsolescence.dev:8080  # Connect to VPS

Then run VT-52 simulator:
  ./vt52 -B telnet localhost 10018
"""

import asyncio
import json
import logging
import signal
import ssl
import sys

try:
    import websockets
except ImportError:
    print("Error: websockets library not found")
    print("Install with: pip install websockets")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Telnet protocol constants
IAC = 0xFF  # Interpret As Command
DO = 0xFD
DONT = 0xFE
WILL = 0xFB
WONT = 0xFC
SB = 0xFA   # Subnegotiation Begin
SE = 0xF0   # Subnegotiation End


class WebSocketBridge:
    """Bridge between TCP (VT-52) and WebSocket (terminal-client)"""

    def __init__(self, tcp_port=10018, ws_url='ws://localhost:8080'):
        self.tcp_port = tcp_port
        self.ws_url = ws_url
        self.server = None
        self.active_connections = 0

    def telnet_escape(self, data):
        """Escape IAC bytes for telnet protocol (0xFF -> 0xFF 0xFF)"""
        # In telnet, literal 0xFF must be sent as 0xFF 0xFF
        escaped = data.replace(b'\xff', b'\xff\xff')
        if escaped != data:
            logger.debug(f"Telnet escape: {len(data)} bytes -> {len(escaped)} bytes")
        return escaped

    def strip_telnet_commands(self, data):
        """Remove telnet protocol commands from data stream

        Some commands are converted to their ASCII equivalents:
        - IAC IP (Interrupt Process) -> Ctrl-C (0x03)
        - IAC BRK (Break) -> Ctrl-Z (0x1A)
        - IAC AO (Abort Output) -> Also might be Ctrl-Z depending on client
        """
        result = bytearray()
        i = 0
        while i < len(data):
            if data[i] == IAC:
                if i + 1 < len(data):
                    if data[i + 1] == IAC:
                        # Escaped IAC (0xFF 0xFF) -> single 0xFF
                        result.append(IAC)
                        i += 2
                    elif data[i + 1] == 0xF3:  # IAC BRK (Break)
                        # Map Break to Ctrl-Z (0x1A)
                        logger.info(f"Telnet BRK -> Ctrl-Z")
                        result.append(0x1A)
                        i += 2
                    elif data[i + 1] == 0xF4:  # IAC IP (Interrupt Process)
                        # Map Interrupt to Ctrl-C (0x03)
                        logger.info(f"Telnet IP -> Ctrl-C")
                        result.append(0x03)
                        i += 2
                    elif data[i + 1] == 0xED:  # IAC AO (Abort Output)
                        # Some terminals send this for Ctrl-Z
                        logger.info(f"Telnet AO -> Ctrl-Z (treating as attention character)")
                        result.append(0x1A)
                        i += 2
                    elif data[i + 1] in (DO, DONT, WILL, WONT):
                        # 3-byte telnet command: IAC DO/DONT/WILL/WONT option
                        if i + 2 < len(data):
                            logger.debug(f"Telnet negotiation: {data[i:i+3].hex()}")
                            i += 3
                        else:
                            i += 2
                    elif data[i + 1] == SB:
                        # Subnegotiation: IAC SB ... IAC SE
                        j = i + 2
                        while j < len(data) - 1:
                            if data[j] == IAC and data[j + 1] == SE:
                                logger.debug(f"Telnet subnegotiation: {data[i:j+2].hex()}")
                                i = j + 2
                                break
                            j += 1
                        else:
                            # Incomplete subnegotiation
                            i = len(data)
                    else:
                        # Other telnet command - log it
                        logger.info(f"Telnet command: {data[i:i+2].hex()}")
                        i += 2
                else:
                    # Incomplete command at end
                    i += 1
            else:
                # Regular data
                result.append(data[i])
                i += 1
        return bytes(result)

    async def handle_telnet_negotiation(self, writer, data):
        """Handle telnet option negotiation (DO/DONT/WILL/WONT)

        Respond to telnet option requests to keep the connection happy.
        We generally refuse all options (WONT/DONT) since we're just a bridge.
        """
        responses = []
        i = 0
        while i < len(data):
            if data[i] == IAC and i + 2 < len(data):
                cmd = data[i + 1]
                if cmd == DO:
                    # Client asks us to DO option -> respond WONT (we refuse)
                    option = data[i + 2]
                    response = bytes([IAC, WONT, option])
                    responses.append(response)
                    logger.info(f"Telnet: Client DO {option} -> Responding WONT {option}")
                elif cmd == DONT:
                    # Client asks us not to DO option -> respond WONT (ok, we won't)
                    option = data[i + 2]
                    response = bytes([IAC, WONT, option])
                    responses.append(response)
                    logger.info(f"Telnet: Client DONT {option} -> Responding WONT {option}")
                elif cmd == WILL:
                    # Client says it WILL do option -> respond DONT (we don't need it)
                    option = data[i + 2]
                    response = bytes([IAC, DONT, option])
                    responses.append(response)
                    logger.info(f"Telnet: Client WILL {option} -> Responding DONT {option}")
                elif cmd == WONT:
                    # Client says it won't do option -> respond DONT (ok, fine)
                    option = data[i + 2]
                    response = bytes([IAC, DONT, option])
                    responses.append(response)
                    logger.info(f"Telnet: Client WONT {option} -> Responding DONT {option}")
            i += 1

        # Send all responses
        if responses:
            for response in responses:
                hex_dump = ' '.join(f'{b:02x}' for b in response)
                logger.info(f">>> Sending telnet response: {hex_dump}")
                writer.write(response)
            await writer.drain()
            logger.info(f"Sent {len(responses)} telnet negotiation response(s)")

    async def handle_vt52_connection(self, reader, writer):
        """Handle incoming TCP connection from VT-52 simulator"""
        client_addr = writer.get_extra_info('peername')
        logger.info(f"VT-52 simulator connected from {client_addr}")
        logger.info(f"TCP connection info: {writer.get_extra_info('socket')}")
        self.active_connections += 1

        ws = None

        try:
            # Connect to terminal-client via WebSocket
            logger.info(f"Connecting to terminal-client at {self.ws_url}...")

            # Set up SSL context for wss:// connections
            ssl_context = None
            if self.ws_url.startswith('wss://'):
                ssl_context = ssl.create_default_context()
                ssl_context.check_hostname = False
                ssl_context.verify_mode = ssl.CERT_NONE
                logger.debug("SSL certificate verification disabled for wss://")

            # Connect with relaxed keepalive settings
            ws = await websockets.connect(
                self.ws_url,
                ssl=ssl_context,
                ping_interval=60,
                ping_timeout=120
            )
            logger.info("✓ Connected to terminal-client")

            # Create tasks for bidirectional relay
            tcp_to_ws_task = asyncio.create_task(
                self.relay_tcp_to_ws(reader, writer, ws)
            )
            ws_to_tcp_task = asyncio.create_task(
                self.relay_ws_to_tcp(ws, writer)
            )

            # Wait for either direction to complete (connection closed)
            done, pending = await asyncio.wait(
                [tcp_to_ws_task, ws_to_tcp_task],
                return_when=asyncio.FIRST_COMPLETED
            )

            # Cancel remaining task
            for task in pending:
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass

        except Exception as e:
            logger.error(f"Bridge error: {e}")
        finally:
            # Clean up
            if ws:
                try:
                    await ws.close()
                except:
                    pass

            try:
                writer.close()
                await writer.wait_closed()
            except:
                pass

            self.active_connections -= 1
            logger.info(f"VT-52 simulator disconnected from {client_addr} ({self.active_connections} active)")

    async def relay_tcp_to_ws(self, reader, writer, ws):
        """Relay data from VT-52 (TCP) to terminal-client (WebSocket)"""
        try:
            while True:
                # Read from TCP connection
                data = await reader.read(4096)
                if not data:
                    logger.debug("TCP connection closed (EOF)")
                    break

                # Log raw bytes received (VERY VERBOSE - shows exactly what VT-52 sent)
                hex_dump = ' '.join(f'{b:02x}' for b in data)
                logger.info(f"<<< Received from VT-52: {len(data)} bytes: {hex_dump}")

                # Handle telnet option negotiation (respond to DO/DONT/WILL/WONT)
                await self.handle_telnet_negotiation(writer, data)

                # Strip telnet protocol commands and convert to control chars
                clean_data = self.strip_telnet_commands(data)
                if len(clean_data) != len(data):
                    logger.info(f"<<< After stripping telnet: {len(clean_data)} bytes")

                if not clean_data:
                    # Only telnet commands, no actual data
                    continue

                # Convert bytes to string (preserving control characters)
                try:
                    # Use 'latin-1' encoding to preserve all bytes as-is (0x00-0xFF)
                    # This ensures control characters like Ctrl-Z (0x1A) are not lost
                    text = clean_data.decode('latin-1')
                except Exception as e:
                    logger.error(f"Error decoding TCP data: {e}")
                    continue

                # Log decoded characters with visual representation
                logger.info(f"<<< Decoded: {repr(text)}")

                # Send as JSON message to WebSocket
                # json.dumps() will properly escape control characters (e.g., \x1a → \u001a)
                msg = {
                    'type': 'input',
                    'data': text
                }
                json_msg = json.dumps(msg)
                logger.info(f">>> Sending to WebSocket: {json_msg[:200]}")
                await ws.send(json_msg)

        except asyncio.CancelledError:
            logger.debug("TCP→WS relay cancelled")
            raise
        except Exception as e:
            logger.error(f"Error in TCP→WS relay: {e}")

    async def relay_ws_to_tcp(self, ws, writer):
        """Relay data from terminal-client (WebSocket) to VT-52 (TCP)"""
        try:
            async for message in ws:
                try:
                    msg = json.loads(message)
                    msg_type = msg.get('type')

                    if msg_type == 'output':
                        # Send output to VT-52
                        data = msg.get('data', '')
                        if data:
                            logger.info(f"<<< Received from server: {repr(data[:100])}")

                            # Encode and send to VT-52 with telnet escaping
                            try:
                                # Check if writer is still open
                                if writer.is_closing():
                                    logger.error("Writer is closing, cannot send data")
                                    break

                                encoded = data.encode('latin-1')
                                # Escape IAC bytes for telnet protocol
                                escaped = self.telnet_escape(encoded)
                                hex_dump = ' '.join(f'{b:02x}' for b in escaped[:80])
                                logger.info(f">>> Sending to VT-52: {len(escaped)} bytes: {hex_dump}...")
                                logger.info(f">>> Repr: {repr(data[:100])}")

                                writer.write(escaped)
                                await writer.drain()
                                logger.info(f">>> Sent successfully, buffer drained")

                            except ConnectionResetError as e:
                                logger.error(f"Connection reset by VT-52: {e}")
                                break
                            except BrokenPipeError as e:
                                logger.error(f"Broken pipe (VT-52 disconnected): {e}")
                                break
                            except Exception as e:
                                logger.error(f"Error sending to VT-52: {e}", exc_info=True)
                                break

                    elif msg_type == 'error':
                        # Log error but don't close connection
                        error_msg = msg.get('data', 'Unknown error')
                        logger.warning(f"Server error: {error_msg}")

                    elif msg_type == 'exit':
                        # Server requested connection close
                        exit_msg = msg.get('data', 'Connection closed')
                        logger.info(f"Server closed connection: {exit_msg}")
                        break

                    else:
                        logger.debug(f"Unknown message type from server: {msg_type}")

                except json.JSONDecodeError:
                    logger.error(f"Invalid JSON from WebSocket: {message[:100]}")
                except Exception as e:
                    logger.error(f"Error processing WebSocket message: {e}")

        except websockets.exceptions.ConnectionClosed:
            logger.debug("WebSocket connection closed")
        except asyncio.CancelledError:
            logger.debug("WS→TCP relay cancelled")
            raise
        except Exception as e:
            logger.error(f"Error in WS→TCP relay: {e}")

    async def start(self):
        """Start TCP server for VT-52 connections"""
        logger.info(f"WebSocket-to-TCP Bridge")
        logger.info(f"  TCP port: {self.tcp_port} (VT-52 simulator)")
        logger.info(f"  WebSocket: {self.ws_url} (terminal-client)")
        logger.info("")
        logger.info(f"Start your VT-52 simulator with:")
        logger.info(f"  ./vt52 -B telnet localhost {self.tcp_port}")
        logger.info("")

        self.server = await asyncio.start_server(
            self.handle_vt52_connection,
            '127.0.0.1',
            self.tcp_port
        )

        logger.info(f"✓ Bridge listening on 127.0.0.1:{self.tcp_port}")
        logger.info("Waiting for VT-52 simulator to connect...")

        async with self.server:
            await self.server.serve_forever()


def parse_args():
    """Parse command-line arguments"""
    tcp_port = 10018  # Default port for VT-52
    ws_url = 'ws://localhost:8080'  # Default WebSocket URL

    if len(sys.argv) > 1:
        try:
            tcp_port = int(sys.argv[1])
        except ValueError:
            print(f"Error: Invalid TCP port '{sys.argv[1]}'")
            sys.exit(1)

    if len(sys.argv) > 2:
        ws_url = sys.argv[2]
        # Validate WebSocket URL
        if not (ws_url.startswith('ws://') or ws_url.startswith('wss://')):
            print(f"Error: WebSocket URL must start with ws:// or wss://")
            sys.exit(1)

    return tcp_port, ws_url


async def main():
    """Main entry point"""
    tcp_port, ws_url = parse_args()

    bridge = WebSocketBridge(tcp_port, ws_url)

    # Handle shutdown gracefully
    loop = asyncio.get_running_loop()

    def signal_handler():
        logger.info("\nShutting down...")
        if bridge.server:
            bridge.server.close()

    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, signal_handler)

    try:
        await bridge.start()
    except KeyboardInterrupt:
        logger.info("\nShutdown requested")
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown...")
