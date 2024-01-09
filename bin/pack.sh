#!/bin/bash

export cmake_compiler="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
if [[ "$1" == "d" || "$1" == "debug" ]]; then
        export build_flag=-DCMAKE_BUILD_TYPE=Debug
else
        ccache --clear
        export build_flag=-DCMAKE_BUILD_TYPE=RelWithDebInfo
fi

cpus=`grep -c ^processor /proc/cpuinfo`
export build_jobs=$[ $cpus / 4 ]

dirs=`ls -d ../*Server ../Benchmark | awk -F/ '{print $2}'`
rm -f $dirs
find ../ -name "*Server.out" -exec rm -rf {} \;
rm -rf ./Benchmark.out
rm -rf ../Benchmark/Benchmark.out

echo "##################################################"
echo "cpus       : " $cpus
echo "build_jobs : " $build_jobs
echo "build_flag : " $build_flag $cmake_compiler
echo "dirs       : " $dirs
echo "##################################################"

build_func(){
        pg=$1
        dir=${1}_build
        rm -rf ../$pg/$dir && mkdir ../$pg/$dir && cd ../$pg/$dir
        cmake $build_flag .. $cmake_compiler
        make -j$build_jobs
        cd ../../bin
        cp ../$pg/${pg}.out ./
        rm -rf ../$pg/$dir

        gzexe $pg.out
        rm $pg.out~
}

export -f build_func

parallel build_func ::: $dirs

# for pg in ${dirs[*]}
# do
#     build_func $pg
# done

unset cmake_compiler
unset build_flag
unset build_jobs
unset build_func
