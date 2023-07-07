#!/bin/bash

pg_list=(
  GameMgrServer
  LobbyServer
  GameServer
  DBServer
  GateServer
)

for pg in ${pg_list[@]}
do
  oldPgCnt=`/bin/ls | grep $pg"_" | wc -l`
  pgCnt=$oldPgCnt
  while true
  do
    ./$pg --logmf=1 -s -d
    sleep 1
    pgCnt=`/bin/ls | grep $pg"_" | wc -l`
    if [ $oldPgCnt = $pgCnt ]; then
      break
    else
      oldPgCnt=$pgCnt
    fi
  done

  pgCnt=`/bin/ls | grep $pg"_" | wc -l`
  if [ 0 = $pgCnt ]; then
    echo -e "\033[31meeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee $pg start fail !!!\033[0m"
  else
    echo -e "\033[32msssssssssssssssssssssssssssssssssssssssssssssssssssssssss $pg start cnt:$pgCnt !!!\033[0m"
  fi
done

echo ""
echo "#########################################################"
ps x | grep Server | grep -v grep
echo "#########################################################"

# vim: fenc=utf8:expandtab:ts=8
