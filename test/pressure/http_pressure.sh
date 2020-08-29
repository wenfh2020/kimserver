#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

[ $# -ne 2 ] && echo './http_pressure.sh 100 100' && exit 1

siege -c $1 -r $2 -H 'Content-Type:application/json' -f ./urls.txt
