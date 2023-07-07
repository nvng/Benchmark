#!/bin/bash

for i in {1..3}
do
  ./Benchmark --logmf=1 -s -d
  sleep 1
done

echo ""
echo "#########################################################"
ps x | grep Benchmark | grep -v grep
echo "#########################################################"
