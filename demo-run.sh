echo "Example script to explain the setup"
echo

read -r -p "Press Enter to start web server..."

echo "First, start the web terminal server. It can run on the VPS but here, we start it only for local use:"
echo
echo "cd web-terminals/simh-server"
cd web-terminals/simh-server

echo "screen -dmS web-server ./start.sh local"
screen -dmS web-server ./start.sh local

cd ../..
echo

read -r -p "Press Enter to start web client..."

echo
echo "now, start the web terminal client that lets the HTML page get its terminal data from the server:"
echo
echo "cd web-terminals/terminal-client"
cd web-terminals/terminal-client

echo "screen -dmS web-client ./start.sh"
screen -dmS web-client ./start.sh

cd ../..
echo

read -r -p "Press Enter to start up Arpanet..."

echo
echo "Next, start up the arpanet so the web terminal has a network to connect to"
cd mini
./arpanet start
cd ..


read -r -p "Done. Press Enter for more info..."
echo
echo "Load arpanet_terminal2.html in your web browser"
echo
echo "Do screen -ls to inspect everything."
echo
echo "To stop the arpanet, do"
echo "./mini/arpanet stop"
echo
echo "To stop the web client/server, "
echo " screen -r web-client, and Ctrl-C"
echo " screen -r web-server, and Ctrl-C"
echo
