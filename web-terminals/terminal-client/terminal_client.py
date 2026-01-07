#!/usr/bin/env python3
"""
ARPANET Terminal Client

Accepts browser connections and proxies them to the simh-server.
Supports multiple concurrent browser sessions (up to 8).
Can run on VPS (with SSL) or laptop (without SSL) - auto-detected.

Architecture:
  Browser 1 → terminal-client (port 8080) ↘
  Browser 2 → terminal-client (port 8080)  → simh-server (port 8081) → simh instances
  Browser N → terminal-client (port 8080) ↗

Usage:
  python3 terminal_client.py                    # Without SSL (laptop)
  python3 terminal_client.py <cert> <key>       # With SSL (VPS)
"""

import asyncio
import json
import logging
import ssl
import sys
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Error: websockets library not found")
    print("Install with: pip install websockets")
    exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class TerminalClient:
    def __init__(self, browser_port=8080, simh_port=8081, certfile=None, keyfile=None):
        self.browser_port = browser_port
        self.simh_port = simh_port
        self.certfile = certfile
        self.keyfile = keyfile
        self.ssl_context = None

        # Session tracking
        self.browser_sessions = {}   # browser_ws → session_id
        self.simh_ws = None           # Connection from simh-server
        self.session_counter = 0

        # Set up SSL context if certificates provided
        if certfile and keyfile:
            self.setup_ssl(certfile, keyfile)

        logger.info(f"TerminalClient initialized")
        logger.info(f"  Browsers will connect on port {browser_port}")
        logger.info(f"  Simh-server will connect on port {simh_port}")
        if self.ssl_context:
            logger.info(f"  SSL/TLS enabled (WSS)")
        else:
            logger.info(f"  Plain WebSocket (WS - no encryption)")

    def setup_ssl(self, certfile, keyfile):
        """Set up SSL context for WSS connections"""
        try:
            self.ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.ssl_context.load_cert_chain(certfile=certfile, keyfile=keyfile)
            logger.info(f"SSL certificate loaded: {certfile}")
            logger.info(f"SSL key loaded: {keyfile}")
        except FileNotFoundError as e:
            logger.error(f"Certificate file not found: {e}")
            logger.error(f"  Expected certfile: {certfile}")
            logger.error(f"  Expected keyfile: {keyfile}")
            sys.exit(1)
        except Exception as e:
            logger.error(f"Error setting up SSL: {e}")
            sys.exit(1)

    def generate_session_id(self):
        """Generate unique session ID"""
        self.session_counter += 1
        timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
        return f"session_{timestamp}_{self.session_counter}"

    async def handle_browser(self, websocket):
        """Handle a browser WebSocket connection"""
        session_id = self.generate_session_id()
        self.browser_sessions[websocket] = session_id

        logger.info(f"Browser connected: {session_id} from {websocket.remote_address}")

        # Notify simh-server of new session
        if self.simh_ws:
            try:
                await self.simh_ws.send(json.dumps({
                    'session': session_id,
                    'type': 'new_session'
                }))
                logger.info(f"Notified simh-server of new session: {session_id}")
            except Exception as e:
                logger.error(f"Failed to notify simh-server: {e}")

        try:
            async for message in websocket:
                await self.relay_from_browser(websocket, session_id, message)
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Browser disconnected: {session_id}")
        except Exception as e:
            logger.error(f"Browser error ({session_id}): {e}")
        finally:
            # Clean up session
            if websocket in self.browser_sessions:
                del self.browser_sessions[websocket]

            # Notify simh-server of session close
            if self.simh_ws:
                try:
                    await self.simh_ws.send(json.dumps({
                        'session': session_id,
                        'type': 'close_session'
                    }))
                    logger.info(f"Notified simh-server of session close: {session_id}")
                except Exception as e:
                    logger.error(f"Failed to notify simh-server of close: {e}")

    async def handle_simh_server(self, websocket):
        """Handle simh-server WebSocket connection"""
        self.simh_ws = websocket
        logger.info(f"Simh-server connected from {websocket.remote_address}")

        try:
            async for message in websocket:
                await self.relay_from_simh_server(message)
        except websockets.exceptions.ConnectionClosed:
            logger.info("Simh-server disconnected")
        except Exception as e:
            logger.error(f"Simh-server error: {e}")
        finally:
            self.simh_ws = None
            logger.warning("Simh-server connection lost - browsers will not receive data")

    async def relay_from_browser(self, browser_ws, session_id, message):
        """Relay message from browser to simh-server with session ID"""
        try:
            msg_data = json.loads(message)
            msg_type = msg_data.get('type', 'unknown')

            logger.debug(f"Browser ({session_id}) → Simh-server: type={msg_type}")

            if self.simh_ws:
                # Add session ID to message
                msg_data['session'] = session_id
                await self.simh_ws.send(json.dumps(msg_data))
            else:
                logger.warning(f"No simh-server connected to relay message from {session_id}")
                # Send error to browser
                await browser_ws.send(json.dumps({
                    'type': 'error',
                    'data': 'Simh server not connected'
                }))

        except json.JSONDecodeError:
            logger.error(f"Invalid JSON from browser ({session_id}): {message[:100]}")
        except Exception as e:
            logger.error(f"Error relaying from browser ({session_id}): {e}")

    async def relay_from_simh_server(self, message):
        """Relay message from simh-server to appropriate browser"""
        try:
            msg_data = json.loads(message)
            session_id = msg_data.get('session')
            msg_type = msg_data.get('type', 'unknown')

            if not session_id:
                logger.error("Message from simh-server missing session ID")
                return

            logger.debug(f"Simh-server → Browser ({session_id}): type={msg_type}")

            # Find browser websocket for this session
            browser_ws = None
            for ws, sid in self.browser_sessions.items():
                if sid == session_id:
                    browser_ws = ws
                    break

            if browser_ws:
                # Remove session ID before sending to browser (browser doesn't need it)
                msg_data.pop('session', None)
                await browser_ws.send(json.dumps(msg_data))
            else:
                # Debug level: This is normal during session cleanup (browser disconnects before simh stops)
                logger.debug(f"No browser found for session {session_id}")

        except json.JSONDecodeError:
            logger.error(f"Invalid JSON from simh-server: {message[:100]}")
        except Exception as e:
            logger.error(f"Error relaying from simh-server: {e}")

    async def start(self):
        """Start both WebSocket servers"""
        protocol = "wss" if self.ssl_context else "ws"

        # Browser server on port 8080
        # Very relaxed keepalive: 60s ping, 120s timeout for maximum stability
        browser_server = await websockets.serve(
            self.handle_browser,
            "0.0.0.0",
            self.browser_port,
            ssl=self.ssl_context,
            ping_interval=60,
            ping_timeout=120
        )
        logger.info(f"Browser server listening on {protocol}://0.0.0.0:{self.browser_port}")

        # Simh-server connection on port 8081
        # Very relaxed keepalive: 60s ping, 120s timeout for maximum stability
        simh_server = await websockets.serve(
            self.handle_simh_server,
            "0.0.0.0",
            self.simh_port,
            ssl=self.ssl_context,
            ping_interval=60,
            ping_timeout=120
        )
        logger.info(f"Simh-server listening on {protocol}://0.0.0.0:{self.simh_port}")

        logger.info("Terminal-client started. Waiting for connections...")

        # Keep running
        await asyncio.Future()


async def main():
    # Parse command-line arguments
    certfile = None
    keyfile = None

    if len(sys.argv) > 1:
        certfile = sys.argv[1]
    if len(sys.argv) > 2:
        keyfile = sys.argv[2]

    server = TerminalClient(browser_port=8080, simh_port=8081, certfile=certfile, keyfile=keyfile)
    await server.start()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server shutting down...")
