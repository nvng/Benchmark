#!/bin/bash

for i in {1..4}
do
  ./GateServer.out --logmf=1 -d
  sleep 1
done

echo ""
echo "#########################################################"
ps x | grep GateServer.out | grep -v grep | wc -l
echo "#########################################################"
