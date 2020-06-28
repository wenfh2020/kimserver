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
    ps -ef | grep -i $name | grep -v grep | awk '{if ($3 > 1) print $3;}' | xargs kill
}

if [ -f $output_file ]; then
    kill_proc $server_name
    cd $work_path/bin
    $output_file config.json
fi
