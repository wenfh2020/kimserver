#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

protoc --version >/dev/null 2>&1
[ $? -ne 0 ] && echo 'pls install protobufs.' && exit 1

cd $work_path/src/core/proto
protoc -I. --cpp_out=. http.proto msg.proto
