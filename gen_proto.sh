#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

protoc --version >/dev/null 2>&1

if [ $? -ne 0 ]; then
    echo 'pls install protobuf'
    exit 1
fi

cd $work_path/src/proto
protoc -I. --cpp_out=. http.proto msg.proto
