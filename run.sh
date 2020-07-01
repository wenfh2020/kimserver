#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server=kimserver
server_file=$work_path/bin/$server

kill_process() {
    name=$1
    processes=$(ps -ef | grep $name | grep -v grep | grep -v log | awk '{if ($2 > 1) print $2;}')
    for p in $processes; do
        kill $p
    done
}

cat_process() {
    sleep 1
    name=$1
    ps -ef | grep -i $name | grep -v grep | grep -v log | awk '{ print $2, $8 }'
}

kill_process $server

[ $# -gt 0 ] && [ $1 == "kill" ] && exit 1

echo '------------'

cd $work_path/src
[ $# -gt 0 ] && [ $1 == "all" ] && make clean
make

res=$?

echo '------------'

if [ $res -eq 0 ]; then
    cd $work_path/bin
    ./$server config.json
    cat_process $server
fi
