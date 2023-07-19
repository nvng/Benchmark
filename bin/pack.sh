#!/bin/bash

if [[ "$1" == "d" || "$1" == "debug" ]]; then
        export build_flag=-DCMAKE_BUILD_TYPE=Debug
else
        export build_flag=-DCMAKE_BUILD_TYPE=Release
        ccache --clear
fi

cpus=`grep -c ^processor /proc/cpuinfo`
export build_jobs=$[ $cpus / 2 ]

dirs=`ls -d ../*Server ../Benchmark | awk -F/ '{print $2}'`
rm -f $dirs

echo "##################################################"
echo "cpus       : " $cpus
echo "build_jobs : " $build_jobs
echo "build_flag : " $build_flag
echo "dirs       : " $dirs
echo "##################################################"

build_func(){
        pg=$1
        dir=${1}_build
        rm -rf ../$pg/$dir && mkdir ../$pg/$dir && cd ../$pg/$dir
        cmake $build_flag ..
        make -j$build_jobs
        cd ../../bin
        cp ../$pg/${pg}.out ./
        rm -rf ../$pg/$dir
}

export -f build_func

parallel build_func ::: $dirs

unset build_flag
unset build_jobs
unset build_func
