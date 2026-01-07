#!/usr/bin/env python3
import asyncio, websockets, socket, sys

MY_ID   = sys.argv[1]   # A or B
PEER_ID = sys.argv[2]
UDP_IN  = int(sys.argv[3])
UDP_OUT = int(sys.argv[4])
VPS     = "ws://obsolescence.dev:8090"
#VPS     = "wss://obsolescence.dev:8090"

udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp.bind(("127.0.0.1", UDP_IN))
udp.setblocking(False)

async def udp_to_ws(ws):
    loop = asyncio.get_running_loop()
    while True:
        data, _ = await loop.sock_recvfrom(udp, 2048)
        frame = (
            bytes([len(MY_ID)]) + MY_ID.encode() +
            bytes([len(PEER_ID)]) + PEER_ID.encode() +
            data
        )
        await ws.send(frame)

async def ws_to_udp(ws):
    async for data in ws:
        pos = 1 + data[0]
        dest_len = data[pos]
        pos += 1 + dest_len
        payload = data[pos:]
        udp.sendto(payload, ("127.0.0.1", UDP_OUT))

async def main():
    async with websockets.connect(VPS, max_size=2**20) as ws:
        await asyncio.gather(
            udp_to_ws(ws),
            ws_to_udp(ws)
        )

asyncio.run(main())

