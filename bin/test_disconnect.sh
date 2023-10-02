#!/bin/bash

while true
do
        ./LobbyServer.out --logmf=1 -s -d
        sleep $((1 + $RANDOM % 30))
        kill -9 `cat .LobbyServer.out_15_400`
        sleep $((1 + $RANDOM % 10))
done
