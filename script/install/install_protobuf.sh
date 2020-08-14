#!/bin/bash

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

file=protobuf-all-4.0.0-rc-2.tar.gz
url=https://github.com/protocolbuffers/protobuf/releases/download/v4.0.0-rc2/$file

insert='i\
if test \"x${ac_cv_env_CFLAGS_set}\" = \"x\"; then : \
    CFLAGS=\"-fPIC\" \
fi \
if test \"x${ac_cv_env_CXXFLAGS_set}\" = \"x\"; then : \
    CXXFLAGS=\"-fPIC\" \
fi
'

insert_func() {
    if [ $(uname -s) == "Darwin" ]; then
        sed -i "" "2665,2670d" configure
        sed -i "" "2665$insert" configure
    else
        sed -i "2665,2670d" configure
        sed -i "2665$insert" configure
    fi
}

wget -c $url
[ $? -ne 0 ] && echo 'download failed!' && exit 1
tar zxf $file
cd protobuf-4.0.0-rc-2
./configure --disable-shared
insert_func
make clean && make uninstall && make -j8 && make install
[ $? -ne 0 ] && exit 1
[ $(uname -s) == "Linux" ] && ldconfig
protoc --version
