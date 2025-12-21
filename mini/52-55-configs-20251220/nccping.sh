#!/bin/bash
#
  echo "NCP=ncp$1 ./ncp-ping -c1 $2"
  NCP=ncp$1 ./ncp-ping -c1 $2
  echo
