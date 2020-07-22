#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

siege -c 50 -r 300 'http://127.0.0.1:3355/kim/im/user/' <./hello.json
