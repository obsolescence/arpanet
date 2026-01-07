cd host126
echo $PWD
cp ./126/rp* .
cp ./126/ds* .
screen -dmS host126 ./pdp10-ka ./mini-run
#screen -dmS host126 ./pdp10-kaov ./mini-run
cd ..

