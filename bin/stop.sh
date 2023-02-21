#!/bin/bash

pg_list=(
  GateServer.out
  LobbyServer.out
  GameServer.out
  GameMgrServer.out
)

sig=${1}
if [ ! -n "$sig" ]; then
  sig=-2
fi

for pg in ${pg_list[@]}
do
  fileNames=`/bin/ls | grep $pg"_"`
  for fn in ${fileNames[@]}
  do
    # echo $fn
    pid=`cat $fn`
    kill $sig $pid

    while true
    do
      sleep 1
      tmp=$(ps -ef | grep $pid | grep -v grep)
      if [ -n "$tmp" ]; then
        echo -e "\033[31mprogram:$pg pid:$pid not exit!!! \033[0m"
      else
        echo -e "\033[32mpid:$pid exit success!!!\033[0m"
        break
      fi
    done
  done
done

echo -e "\033[33mall program exit success!!!\033[0m"

# vim: fenc=utf8:expandtab:ts=8
