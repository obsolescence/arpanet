#setup:
#python3 -m venv venv
#source venv/bin/activate
#pip install --upgrade pip
#pip install websockets

source venv/bin/activate

python3 udp_ws_client.py B A 11331 11162
