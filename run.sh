#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server_name=kimserver
output_file=$work_path/bin/$server_name

cd $work_path/src
make

kill_proc() {
    name=$1
    ps -ef | grep $name | grep -v grep | grep -v log | awk '{if ($2 > 1) print $2;}' | xargs kill
}

kill_proc $server_name

if [ -f $output_file ]; then
    cd $work_path/bin
    $output_file config.json
fi
