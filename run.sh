#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server_name=kimserver
output_file=$work_path/bin/$server_name

[ -f $output_file ] && rm -f output_file
cd $work_path/src
make

kill_process() {
    name=$1
    ps -ef | grep $name | grep -v grep | grep -v log | awk '{if ($2 > 1) print $2;}' | xargs kill
}

cat_process() {
    sleep 1
    name=$1
    ps -ef | grep -i $name | grep -v grep | grep -v log | awk '{ print $2, $NF }'
}

kill_process $server_name

[ $# -gt 0 ] && [ $1 == "kill" ] && exit 1

if [ -f $output_file ]; then
    cd $work_path/bin
    $output_file config.json
    cat_process $server_name
fi
