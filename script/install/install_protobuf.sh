#!/bin/bash

file=protobuf-all-4.0.0-rc-2.tar.gz
url=https://github.com/protocolbuffers/protobuf/releases/download/v4.0.0-rc2/$file

wget -c $url
[ $? -ne 0 ] && echo 'download failed!' && exit 1
tar zxf $file
cp protobuf_configrue protobuf-4.0.0-rc-2
cd protobuf-4.0.0-rc-2
mv protobuf_configrue configrue
./configrue --disable-shared
make clean
make uninstall
make -j8 && make install
[ $? -ne 0 ] && exit 1
[ $(uname -s) == "Linux" ] && ldconfig
protoc --version
