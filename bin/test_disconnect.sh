#!/bin/bash

while true
do
        ./LobbyServer.out --logmf=1 -s -d
        # sleep $((1 + $RANDOM % 30))

        random_number=`echo "scale=4 ; ${RANDOM} / 32767 * 3" | bc -l`
        echo "11111111111111111111111111111 $random_number"
        sleep $random_number

        # kill -9 `cat .LobbyServer.out_15_400`
        killall -9 LobbyServer.out
        # sleep $((1 + $RANDOM % 10))
done
