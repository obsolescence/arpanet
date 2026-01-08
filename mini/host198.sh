cd host198
echo $PWD
cp ./306/rp* .
cp ./306/ds* .
echo screen -dmS host198 ./pdp10-ka ./mini-run
screen -dmS host198 ./pdp10-ka ./mini-run
cd ..
