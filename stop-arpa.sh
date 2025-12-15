#!/bin/bash

cd mini
./stop
cd ..

screen -S terminal-client -X stuff $'\003'
screen -S simh-server -X stuff $'\003'

echo "There should not be any screen sessions left, or kill them manually:"
screen -ls
