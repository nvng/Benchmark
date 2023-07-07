#!/bin/bash

for i in {1..4}
do
  ./LobbyServer.out --logmf=1 -d
  sleep 1
done

echo ""
echo "#########################################################"
ps x | grep LobbyServer.out | grep -v grep | wc -l
echo "#########################################################"
