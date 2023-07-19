#!/bin/bash

pg_list=(
  GateServer.out
  LobbyServer.out
  DBServer.out
  GameServer.out
  GameMgrServer.out
)

stop_sig=$1
if [ ! -n "$stop_sig" ]; then
  stop_sig=-2
fi

stop_server_func(){
        if [ "$2" == "" ]
        then
                return
        fi

        pg=$1
        fn=$2
        pid=`cat $fn`
        kill $stop_sig $pid
        while true
        do
                sleep 1
                tmp=`ps -ef | grep $pid | grep $pg | grep -v grep`
                if [ -n "$tmp" ]; then
                        echo -e "\033[31mprogram:$pg pid:$pid not exit!!! \033[0m"
                else
                        echo -e "\033[32mpid:$pid exit success!!!\033[0m"
                        break
                fi
        done
}

export stop_sig
export -f stop_server_func

for pg in ${pg_list[*]}
do
        fileNames=`/bin/ls | grep $pg"_"`
        if [ "$fileNames" != "" ]
        then
                echo "begin stop server :" $pg
                parallel stop_server_func ::: $pg ::: $fileNames
        fi
done

unset stop_server_func
unset stop_sig

echo -e "\033[33mall program exit success!!!\033[0m"

# vim: fenc=utf8:expandtab:ts=8
