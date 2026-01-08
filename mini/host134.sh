cd host134
echo $PWD
cp ./134/rp* .
cp ./134/ds* .
echo screen -dmS host134 ./pdp10-ka ./mini-run
screen -dmS host134 ./pdp10-ka ./mini-run
cd ..
