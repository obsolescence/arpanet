#!/usr/bin/env bash

#echo $1 $2
#echo -----

# Argument 1: IMP number (default 52)
if [[ "$1" =~ ^[0-9]+$ ]]; then
    IMP="$1"
else
    IMP=52
fi

# Argument 2: host selector (default 0)
#if [[ -n "$2" ]]; then
if [[ "$2" == "1" ]]; then
    HOST="-p $2"
else
    HOST=""
fi

#echo "IMP=$IMP"
#echo "HOST=$HOST"

#echo NCP=./"ncp$IMP" ./ncp-telnet $HOST -c 126
NCP=./"ncp$IMP" ./ncp-telnet -c $HOST 126
