#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

siege -c 100 -r 100 -H 'Content-Type:application/json' -f ./urls.txt
