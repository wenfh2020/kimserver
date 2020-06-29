#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server_name=kimserver
output_file=$work_path/bin/$server_name

kill_process() {
    name=$1
    processes=$(ps -ef | grep $name | grep -v grep | grep -v log | awk '{if ($2 > 1) print $2;}')
    for p in $processes; do
        kill $p
    done
}

kill_process $server_name

[ $# -gt 0 ] && [ $1 == "kill" ] && exit 1

echo '------------'

cat_process() {
    sleep 1
    name=$1
    ps -ef | grep -i $name | grep -v grep | grep -v log | awk '{ print $2, $8 }'
}

[ -f $output_file ] && rm -f output_file
cd $work_path/src
make

if [ -f $output_file ]; then
    cd $work_path/bin
    $output_file config.json
    cat_process $server_name
fi
