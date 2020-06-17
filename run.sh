#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server_name=kim_server
output_file=$work_path/bin/$server_name

cd $work_path/src
make

if [ -f $output_file ]; then
    cd $work_path/bin $output_file config.json
fi
