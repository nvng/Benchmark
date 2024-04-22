#!/bin/bash

export cmake_compiler="-DCMAKE_CXX_COMPILER=clang++"
# export cmake_compiler="-DCMAKE_CXX_COMPILER=g++"
if [[ "$1" == "d" || "$1" == "debug" ]]; then
        build_type=$1
        export build_flag="-DCMAKE_BUILD_TYPE=Debug -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE -Djemalloc_LIBRARIES=libjemalloc.a"
        # export build_flag=-DCMAKE_BUILD_TYPE=Debug
else
        build_type=9999
        export build_flag="-DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE -Djemalloc_LIBRARIES=libjemalloc.a"
        # export build_flag=-DCMAKE_BUILD_TYPE=RelWithDebInfo
        ccache --clear
fi

if [[ `uname` == 'Darwin' ]]; then
    echo "Mac OS"
    cpus=8
    export build_jobs=$cpus
elif [[ `uname` == 'Linux' ]]; then
    echo "Linux"
    cpus=`grep -c ^processor /proc/cpuinfo`
    export build_jobs=$[ $cpus / 4 ]
fi

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
        cp ../$pg/$pg.out ./
        rm -rf ../$pg/$dir

        # gzexe $pg.out
        # rm $pg.out~
}

export -f build_func

if [ $NVNGLIB_INCLUDE_DIR ]; then
        cd $NVNGLIB_INCLUDE_DIR && ./pack.sh $build_type $cmake_compiler && cd -
fi

if [[ `uname` == 'Darwin' ]]; then
    echo "Mac OS"

    for pg in ${dirs[*]}
    do
            build_func $pg
    done
elif [[ `uname` == 'Linux' ]]; then
    echo "Linux"

    if [ "$cmake_compiler" == "-DCMAKE_CXX_COMPILER=g++" ]; then
            export build_jobs=$[ $cpus / 2 ]
            for pg in ${dirs[*]}
            do
                    build_func $pg
            done
    else
            parallel build_func ::: $dirs
    fi
fi

unset build_flag
unset build_jobs
unset build_func
