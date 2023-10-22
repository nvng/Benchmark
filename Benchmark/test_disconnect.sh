#!/bin/bash

while true
do
        ./start.sh
        sleep $((1 + $RANDOM % 30))
        ./stop.sh
        sleep $((1 + $RANDOM % 10))
done
