#!/usr/bin/env python3
"""
ARPANET Simh Server

Manages up to 8 concurrent simh-pdp1 instances for terminal users.
Connects to terminal-client and handles multiplexed session messages.

Architecture:
  terminal-client ← simh-server → simh instance 1
                                → simh instance 2
                                → simh instance N (max 8)

Usage:
  python3 simh_server.py <terminal_client_url> [script_path]

Examples:
  python3 simh_server.py ws://localhost:8081 ./do.sh
  python3 simh_server.py wss://obsolescence.dev:8081 ./do.sh
"""

import asyncio
import json
import sys
import os
import ssl
import logging
import signal
import subprocess
import fcntl
import termios
import struct
import select
import time

try:
    import websockets
except ImportError:
    print("Error: websockets library not found")
    print("Install with: pip install websockets")
    sys.exit(1)

try:
    import pty
except ImportError:
    print("Error: pty module not available (required for terminal emulation)")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def safe_kill_process_group(proc):
    """Safely kill a process and its entire process group"""
    if not proc or proc.poll() is not None:
        return  # Process already dead

    try:
        # Kill the entire process group
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        proc.wait(timeout=2)
    except:
        try:
            proc.terminate()
            proc.wait(timeout=2)
        except:
            try:
                proc.kill()
            except:
                pass


class ConnectionState:
    """Connection state constants"""
    DISCONNECTED = "disconnected"
    CONNECTING = "connecting"
    CONNECTED = "connected"


class TerminalClientConnection:
    """Represents a connection to one terminal-client instance"""
    def __init__(self, url, connection_id):
        self.url = url
        self.connection_id = connection_id
        self.ws = None
        self.state = ConnectionState.DISCONNECTED
        self.retry_count = 0
        self.max_backoff = 16  # Max 16 seconds between retries
        self.listener_task = None

    @property
    def connected(self):
        """Backward compatibility: connected property based on state"""
        return self.state == ConnectionState.CONNECTED

    def __repr__(self):
        return f"<TerminalClientConnection {self.connection_id} ({self.url}) - {self.state}>"


class SimhSession:
    """Represents one simh instance and its associated session"""
    def __init__(self, session_id, script_path, session_number):
        self.session_id = session_id
        self.script_path = script_path
        self.session_number = session_number  # 0-7 for IMP/host selection
        self.proc = None
        self.master_fd = None
        self.baud_rate = 9600  # Default, can be changed
        self.running = False

    def spawn(self):
        """Spawn simh in a pty"""
        try:
            # Spawn bash running do.sh in a pty
            self.master_fd, slave_fd = pty.openpty()

            # Set pty size
            fcntl.ioctl(slave_fd, termios.TIOCSWINSZ,
                       struct.pack('HHHH', 24, 80, 0, 0))

            # Prepare environment with SESSION_NUMBER for IMP/host selection
            env = os.environ.copy()
            env['SESSION_NUMBER'] = str(self.session_number)

            self.proc = subprocess.Popen(
                ['bash', self.script_path],
                stdin=slave_fd,
                stdout=slave_fd,
                stderr=slave_fd,
                env=env,
                preexec_fn=os.setsid,
                cwd=os.path.dirname(os.path.abspath(self.script_path))
            )

            os.close(slave_fd)
            self.running = True
            logger.info(f"Spawned simh for session {self.session_id} with SESSION_NUMBER={self.session_number} (PID {self.proc.pid})")
            return True
        except Exception as e:
            logger.error(f"Failed to spawn simh for {self.session_id}: {e}")
            return False

    def cleanup(self):
        """Clean up simh process and PTY"""
        self.running = False

        # Try graceful exit first by sending Ctrl-] to telnet client
        if self.master_fd and self.proc and self.proc.poll() is None:
            try:
                # Send Ctrl-] (ASCII 29) to telnet client to trigger graceful exit
                os.write(self.master_fd, b'\x1d')
                # Give telnet client time to process exit command
                time.sleep(0.3)

                # Check if process exited gracefully
                if self.proc.poll() is None:
                    # Still running, send newline to confirm exit
                    os.write(self.master_fd, b'\n')
                    time.sleep(0.2)

                logger.debug(f"Sent Ctrl-] for graceful exit ({self.session_id})")
            except Exception as e:
                logger.debug(f"Error sending Ctrl-] ({self.session_id}): {e}")

        # Force kill if still running
        if self.proc:
            safe_kill_process_group(self.proc)
            self.proc = None
        if self.master_fd:
            try:
                os.close(self.master_fd)
            except:
                pass
            self.master_fd = None
        logger.info(f"Cleaned up session {self.session_id}")


class SimhServer:
    def __init__(self, terminal_client_urls, script_path='./do.sh'):
        self.terminal_client_urls = terminal_client_urls if isinstance(terminal_client_urls, list) else [terminal_client_urls]
        self.script_path = script_path
        self.connections = []  # List of TerminalClientConnection objects
        self.sessions = {}  # session_id → SimhSession
        self.session_to_connection = {}  # session_id → TerminalClientConnection
        self.max_sessions = 8  # Global limit across all connections

        # Session number pool (0-7) for IMP/host assignment
        # 4 IMPs × 2 hosts each = 8 concurrent sessions
        self.available_session_numbers = set(range(8))  # {0,1,2,3,4,5,6,7}
        self.session_numbers = {}  # session_id → session_number

    def _get_connection_id(self, url):
        """Generate friendly name for connection based on URL"""
        if 'localhost' in url or '127.0.0.1' in url:
            return "local"
        elif 'obsolescence.dev' in url:
            return "vps"
        else:
            # Extract hostname from URL
            try:
                hostname = url.split('://')[1].split(':')[0]
                return hostname
            except:
                return url

    async def _connect_to_terminal_client(self, connection):
        """Connect to a specific terminal-client"""
        try:
            connection.state = ConnectionState.CONNECTING

            # For WSS connections, disable certificate verification
            ssl_context = None
            if connection.url.startswith('wss://'):
                ssl_context = ssl.create_default_context()
                ssl_context.check_hostname = False
                ssl_context.verify_mode = ssl.CERT_NONE
                logger.debug(f"SSL certificate verification disabled for {connection.connection_id}")

            # Very relaxed keepalive: 60s ping, 120s timeout for maximum stability
            # Tolerates CPU load, WiFi hiccups, network delays
            connection.ws = await websockets.connect(
                connection.url,
                ssl=ssl_context,
                ping_interval=60,
                ping_timeout=120
            )
            connection.state = ConnectionState.CONNECTED
            logger.info(f"✓ Connected to {connection.connection_id}: {connection.url}")
            return True
        except Exception as e:
            logger.debug(f"✗ Connection attempt failed for {connection.connection_id}: {e}")
            connection.state = ConnectionState.DISCONNECTED
            connection.ws = None
            return False

    async def initialize_connections(self):
        """Initialize connections to all terminal-clients (initial attempt only)"""
        for i, url in enumerate(self.terminal_client_urls):
            conn_id = self._get_connection_id(url)
            connection = TerminalClientConnection(url, conn_id)
            self.connections.append(connection)

            # Try initial connection (but don't block if offline - maintenance task will retry)
            await self._connect_to_terminal_client(connection)

        # Report initial connection status
        connected_count = sum(1 for c in self.connections if c.connected)
        if connected_count == 0:
            logger.warning("No terminal-clients connected initially.")
            logger.warning("Connection monitors will keep trying to connect...")
        else:
            logger.info(f"Initially connected to {connected_count}/{len(self.connections)} terminal-client(s)")

        return True  # Always succeed - maintenance tasks will handle reconnection

    async def maintain_connection(self, connection):
        """Continuously monitor and maintain connection, reconnecting if needed"""
        logger.info(f"Starting connection monitor for {connection.connection_id}")

        while True:
            try:
                if connection.state == ConnectionState.DISCONNECTED:
                    # Calculate backoff delay
                    if connection.retry_count > 0:
                        backoff = min(2 ** connection.retry_count, connection.max_backoff)
                        logger.info(f"Reconnecting to {connection.connection_id} in {backoff}s (attempt #{connection.retry_count + 1})")
                        await asyncio.sleep(backoff)

                    # Try to connect
                    logger.info(f"Attempting to connect to {connection.connection_id} ({connection.url})...")
                    success = await self._connect_to_terminal_client(connection)

                    if success:
                        # Connection established, start listener
                        logger.info(f"Starting listener for {connection.connection_id}")
                        connection.listener_task = asyncio.create_task(
                            self.listen_to_connection(connection)
                        )
                        connection.retry_count = 0  # Reset retry count on success
                    else:
                        # Connection failed, increment retry count
                        connection.retry_count += 1

                elif connection.state == ConnectionState.CONNECTED:
                    # Check if listener task is running
                    if connection.listener_task is None or connection.listener_task.done():
                        # No listener running - start one (happens when initial connection succeeded)
                        logger.info(f"Starting listener for {connection.connection_id}")
                        connection.listener_task = asyncio.create_task(
                            self.listen_to_connection(connection)
                        )

                    # Connection is healthy, wait
                    await asyncio.sleep(5)

                elif connection.state == ConnectionState.CONNECTING:
                    # Currently connecting, wait a bit
                    await asyncio.sleep(1)

                else:
                    # Unknown state, wait and retry
                    await asyncio.sleep(1)

            except asyncio.CancelledError:
                # Clean shutdown
                logger.info(f"Connection monitor for {connection.connection_id} cancelled")
                break
            except Exception as e:
                logger.error(f"Error in connection monitor for {connection.connection_id}: {e}")
                await asyncio.sleep(5)

        logger.info(f"Connection monitor for {connection.connection_id} stopped")

    async def handle_message(self, message, source_connection):
        """Handle incoming message from terminal-client"""
        try:
            msg = json.loads(message)
            session_id = msg.get('session')
            msg_type = msg.get('type')

            if not session_id:
                logger.error("Message missing session ID")
                return

            if msg_type == 'new_session':
                # Map this session to the source connection
                self.session_to_connection[session_id] = source_connection
                await self.create_session(session_id)

            elif msg_type == 'close_session':
                # Clean up mapping
                self.session_to_connection.pop(session_id, None)
                await self.destroy_session(session_id)

            elif msg_type == 'input':
                await self.handle_input(session_id, msg.get('data', ''))

            elif msg_type == 'resize':
                await self.handle_resize(session_id, msg.get('cols', 80), msg.get('rows', 24))

            elif msg_type == 'setBaudRate':
                await self.handle_baud_rate(session_id, msg.get('baudRate', 9600))

            else:
                logger.warning(f"Unknown message type: {msg_type}")

        except json.JSONDecodeError:
            logger.error(f"Invalid JSON: {message[:100]}")
        except Exception as e:
            logger.error(f"Error handling message: {e}")

    async def create_session(self, session_id):
        """Create new simh session"""
        if len(self.sessions) >= self.max_sessions:
            logger.warning(f"Session limit reached ({self.max_sessions}), rejecting {session_id}")
            # Send busy message to terminal-client
            await self.send_to_terminal({
                'session': session_id,
                'type': 'error',
                'data': f'All {self.max_sessions} terminals are busy. Please try again later.'
            })
            # Close session immediately
            await self.send_to_terminal({
                'session': session_id,
                'type': 'exit',
                'data': 'Connection closed - terminal busy'
            })
            return

        if session_id in self.sessions:
            logger.warning(f"Session {session_id} already exists")
            return

        # Allocate session number from pool (0-7)
        if not self.available_session_numbers:
            logger.error(f"No available session numbers (pool empty)")
            await self.send_to_terminal({
                'session': session_id,
                'type': 'error',
                'data': 'Internal error: no available session slots'
            })
            return

        session_number = min(self.available_session_numbers)  # Get lowest available number
        self.available_session_numbers.remove(session_number)
        self.session_numbers[session_id] = session_number

        logger.info(f"Allocated session number {session_number} for {session_id}")

        # Create and spawn new session
        session = SimhSession(session_id, self.script_path, session_number)
        if session.spawn():
            self.sessions[session_id] = session
            logger.info(f"Created session {session_id} ({len(self.sessions)}/{self.max_sessions})")

            # Start relay task for this session
            asyncio.create_task(self.relay_simh_to_terminal(session_id))
        else:
            # Spawn failed, return session number to pool
            logger.error(f"Failed to create session {session_id}, returning session number {session_number} to pool")
            self.available_session_numbers.add(session_number)
            del self.session_numbers[session_id]
            await self.send_to_terminal({
                'session': session_id,
                'type': 'error',
                'data': 'Failed to start simulator'
            })

    async def destroy_session(self, session_id):
        """Destroy simh session"""
        if session_id in self.sessions:
            session = self.sessions[session_id]
            session.cleanup()
            del self.sessions[session_id]

            # Return session number to pool
            if session_id in self.session_numbers:
                session_number = self.session_numbers.pop(session_id)
                self.available_session_numbers.add(session_number)
                logger.info(f"Destroyed session {session_id}, returned session number {session_number} to pool ({len(self.sessions)}/{self.max_sessions})")
            else:
                logger.warning(f"Destroyed session {session_id} but no session number found in mapping")
                logger.info(f"Destroyed session {session_id} ({len(self.sessions)}/{self.max_sessions})")

    async def handle_input(self, session_id, data):
        """Write user input to simh PTY"""
        if session_id in self.sessions:
            session = self.sessions[session_id]
            if session.master_fd:
                try:
                    os.write(session.master_fd, data.encode('utf-8'))
                except Exception as e:
                    logger.error(f"Error writing to PTY ({session_id}): {e}")

    async def handle_resize(self, session_id, cols, rows):
        """Handle terminal resize"""
        if session_id in self.sessions:
            session = self.sessions[session_id]
            if session.master_fd:
                try:
                    fcntl.ioctl(session.master_fd, termios.TIOCSWINSZ,
                              struct.pack('HHHH', rows, cols, 0, 0))
                    logger.info(f"Resized terminal ({session_id}) to {cols}x{rows}")
                except Exception as e:
                    logger.error(f"Error resizing PTY ({session_id}): {e}")

    async def handle_baud_rate(self, session_id, baud_rate):
        """Handle baud rate setting"""
        if session_id in self.sessions:
            session = self.sessions[session_id]
            session.baud_rate = baud_rate
            cps = baud_rate / 10.0
            delay_per_char = 1.0 / cps if cps > 0 else 0
            logger.info(f"Set baud rate ({session_id}) to {baud_rate} ({cps:.1f} CPS, {delay_per_char*1000:.1f}ms per char)")

    async def relay_simh_to_terminal(self, session_id):
        """Read from simh PTY and send to terminal-client with baud rate limiting"""
        if session_id not in self.sessions:
            return

        session = self.sessions[session_id]

        # Check if this session has a connected terminal-client
        def is_connection_alive():
            conn = self.session_to_connection.get(session_id)
            return conn and conn.connected and conn.ws

        while session.running and session.proc and session.proc.poll() is None and is_connection_alive():
            try:
                # Check if data is available on the PTY
                ready, _, _ = select.select([session.master_fd], [], [], 0.1)

                if ready:
                    # Read from pty
                    data = os.read(session.master_fd, 4096)
                    if data and is_connection_alive():
                        # Apply baud rate limiting
                        data_str = data.decode('utf-8', errors='replace')
                        chars_per_second = session.baud_rate / 10.0
                        chunk_size = max(1, int(chars_per_second / 10))

                        for i in range(0, len(data_str), chunk_size):
                            # Check connection before each chunk (prevents race condition during session close)
                            if not is_connection_alive():
                                logger.debug(f"Connection lost during chunk send for {session_id}, aborting relay")
                                break

                            chunk = data_str[i:i+chunk_size]
                            await self.send_to_terminal({
                                'session': session_id,
                                'type': 'output',
                                'data': chunk
                            })

                            # Calculate delay for this chunk
                            delay = len(chunk) / chars_per_second if chars_per_second > 0 else 0
                            if delay > 0:
                                await asyncio.sleep(delay)

                await asyncio.sleep(0.01)

            except Exception as e:
                logger.error(f"Error reading from PTY ({session_id}): {e}")
                break

        # Session ended
        logger.info(f"Relay ended for session {session_id}")
        if session_id in self.sessions:
            await self.destroy_session(session_id)

    async def send_to_terminal(self, msg):
        """Send message to correct terminal-client based on session"""
        session_id = msg.get('session')

        # Find which connection owns this session
        connection = self.session_to_connection.get(session_id)

        if connection and connection.ws and connection.connected:
            try:
                await connection.ws.send(json.dumps(msg))
            except Exception as e:
                logger.error(f"Failed to send to {connection.connection_id}: {e}")
                connection.connected = False
        else:
            if session_id:
                logger.error(f"No connected terminal-client for session {session_id}")
            else:
                logger.error(f"Message missing session ID, cannot route")

    async def listen_to_connection(self, connection):
        """Listen to messages from one terminal-client"""
        if not connection.ws:
            return

        logger.info(f"Listener active for {connection.connection_id}")

        try:
            async for message in connection.ws:
                await self.handle_message(message, connection)
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Terminal-client disconnected: {connection.connection_id}")
        except Exception as e:
            logger.error(f"WebSocket error with {connection.connection_id}: {e}")
        finally:
            # Mark as disconnected (triggers reconnection)
            connection.state = ConnectionState.DISCONNECTED
            connection.ws = None

            # Clean up sessions from this connection
            sessions_to_remove = [sid for sid, conn in self.session_to_connection.items() if conn == connection]
            if sessions_to_remove:
                logger.info(f"Cleaning up {len(sessions_to_remove)} session(s) from disconnected {connection.connection_id}")
                for session_id in sessions_to_remove:
                    await self.destroy_session(session_id)
                    self.session_to_connection.pop(session_id, None)

            logger.info(f"Listener stopped for {connection.connection_id}")

    async def run(self):
        """Main run loop with automatic reconnection"""
        # Initialize connections (tries once, doesn't fail if offline)
        await self.initialize_connections()

        # Start maintenance task for EACH connection (even offline ones)
        # These tasks run forever and handle reconnection
        maintenance_tasks = []
        for conn in self.connections:
            task = asyncio.create_task(self.maintain_connection(conn))
            maintenance_tasks.append(task)

        logger.info(f"Started {len(maintenance_tasks)} connection monitor(s)")
        logger.info("Simh-server is running. Press Ctrl+C to stop.")

        try:
            # Run all maintenance tasks concurrently (they run forever)
            await asyncio.gather(*maintenance_tasks, return_exceptions=True)
        except KeyboardInterrupt:
            logger.info("Interrupted by user")
        finally:
            # Cancel all maintenance tasks
            for task in maintenance_tasks:
                task.cancel()
            await self.cleanup()

        return True

    async def cleanup(self):
        """Clean up all resources"""
        logger.info("Cleaning up...")

        # Close all sessions
        for session_id in list(self.sessions.keys()):
            await self.destroy_session(session_id)

        # Close all connections
        for connection in self.connections:
            if connection.ws:
                try:
                    await connection.ws.close()
                    logger.info(f"Closed connection to {connection.connection_id}")
                except:
                    pass


async def main():
    if len(sys.argv) < 2:
        print(f"""
ARPANET Simh Server

Usage:
  python3 simh_server.py <terminal_client_url> [terminal_client_url2] [...] [script_path]

Arguments:
  terminal_client_url  WebSocket URL(s) of terminal-client(s)
                       Use ws:// for local connections
                       Use wss:// for VPS/SSL connections
  script_path          Path to do.sh script (default: ./do.sh)
                       Optional, must be last argument if provided

Examples:
  # Single connection (backward compatible):
  python3 simh_server.py ws://localhost:8081

  # Multiple connections (local + VPS):
  python3 simh_server.py ws://localhost:8081 wss://obsolescence.dev:8081

  # With custom script path:
  python3 simh_server.py ws://localhost:8081 wss://obsolescence.dev:8081 /path/to/do.sh

The simh-server will:
1. Connect to one or more terminal-clients
2. Accept multiplexed session messages from all sources
3. Spawn separate simh-pdp1 instance for each session (max 8 total)
4. Relay terminal I/O between browsers and simh instances
5. Continue running even if some terminal-clients are offline
        """)
        return

    # Parse arguments: URLs are ws:// or wss://, script path is a file path
    args = sys.argv[1:]
    terminal_client_urls = []
    script_path = './do.sh'

    for arg in args:
        if arg.startswith('ws://') or arg.startswith('wss://'):
            terminal_client_urls.append(arg)
        else:
            # Assume it's the script path
            script_path = arg

    if not terminal_client_urls:
        print("Error: At least one terminal-client URL required")
        sys.exit(1)

    # Verify script exists
    if not os.path.exists(script_path):
        print(f"Error: Script not found: {script_path}")
        sys.exit(1)

    logger.info(f"Starting Simh Server")
    logger.info(f"  Terminal-client(s): {', '.join(terminal_client_urls)}")
    logger.info(f"  Script: {script_path}")
    logger.info(f"  Max sessions: 8 (global limit)")

    server = SimhServer(terminal_client_urls, script_path)
    success = await server.run()

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown...")
