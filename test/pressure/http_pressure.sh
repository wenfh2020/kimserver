#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

siege -c 100 -r 100 --header 'Content-Type:application/json' 'http://127.0.0.1:3355/kim/im/user/' <./hello.json
