#!/bin/bash

for i in {1..4}
do
  ./Benchmark.out --logmf=1 -d
  sleep 1
done
