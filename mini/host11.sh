cd host11
echo $PWD
#cp ./011/* .
echo not copying fresh system disks, assumed unnecessary for WAITS

echo screen -dmS host11 ./pdp10-ka ./waits.ini
screen -dmS host11 ./pdp10-ka ./waits.ini
sleep 2
screen -S host11 -p 0 -X clear
sleep 2
screen -S host11 -p 0 -X clear
screen -S host11 -p 0 -X stuff $'R INFO\r'
cd ..

