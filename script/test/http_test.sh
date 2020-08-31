#!/bin/sh

work_path=$(dirname $0)
cd $work_path
work_path=$(pwd)

url=""
request=""
addr="127.0.0.1:3355"
base_url="http://$addr"

if [ $# -le 0 ]; then
    echo "more param"
    exit 1
fi

case "$1" in
"help")
    echo "1 - hello"
    exit 0
    ;;
"1")
    url="$base_url/kim/helloworld/"
    request='{"uid":"hello world"}'
    ;;
"2")
    url="$base_url/kim/test_cmd/"
    request='{"test":"test_cmd"}'
    ;;
"3")
    url="$base_url/kim/test_redis/"
    request='{"key": "key123", "value": "hello_world"}'
    ;;
"4")
    url="$base_url/kim/test_timeout/"
    request='{"key": "test_timeout"}'
    ;;
"5")
    url="$base_url/kim/test_mysql/"
    request='{"id": "1", "value": "hello_world"}'
    ;;
"6")
    url="$base_url/kim/test_mysql/"
    request='{"id": "1", "value": "hello_world", "session": true }'
    ;;
*)
    echo "invalid param"
    exit 1
    ;;
esac

echo $request
echo ""

# curl -v -d "$request" -H 'Connection:keep-alive'  $url | python -m json.tool
curl -v -d "$request" $url | python -m json.tool

#| ascii2uni -a U -q
