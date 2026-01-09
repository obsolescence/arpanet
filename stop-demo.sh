cd mini
./arpanet stop
cd ..

screen -S web-client -X stuff $'\003'
screen -S web-server -X stuff $'\003'
screen -S waitsconnect -X stuff $'\003'

echo "Stopped everything. To check, screen -ls should show no screen sessions:"
sleep 1
echo "screen -ls"
screen -ls
echo
echo "Done."



