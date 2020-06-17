#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

server_name=kim_server
output_file=$work_path/bin/$server_name

src_path=$work_path/src
cd $src_path

g++ -g log.cpp server.cpp -o $output_file && $output_file
