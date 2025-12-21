cd host06
echo $PWD
cp ./006/rp* .
cp ./006/ds* .
echo screen -dmS host06 ./pdp10-ka ./mini-run
screen -dmS host06 ./pdp10-ka ./mini-run
cd ..

