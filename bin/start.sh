#!/bin/bash

rm -rf core_*

pg_list=(
        GameMgrServer.out
        GameServer.out
        DBServer.out
        LobbyServer.out
        GateServer.out
)

start_server_func(){
        pg=$1
        oldPgCnt=`/bin/ls -a | grep $pg"_" | wc -l`
        pgCnt=$oldPgCnt
        while true
        do
                ./$pg --logmf=1 -s -d
                sleep 1
                pgCnt=`/bin/ls -a | grep $pg"_" | wc -l`
                if [ $oldPgCnt = $pgCnt ]; then
                        break
                else
                        oldPgCnt=$pgCnt
                fi
        done

        pgCnt=`/bin/ls -a | grep $pg"_" | wc -l`
        if [ 0 = $pgCnt ]; then
                echo -e "\033[31meeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee $pg start fail !!!\033[0m"
        else
                echo -e "\033[32msssssssssssssssssssssssssssssssssssssssssssssssssssssssss $pg start cnt:$pgCnt !!!\033[0m"
        fi
}

export -f start_server_func

parallel start_server_func ::: ${pg_list[*]}

unset start_server_func

echo ""
echo "#########################################################"
ps x | grep Server | grep -v grep
echo "#########################################################"

# vim: fenc=utf8:expandtab:ts=8
