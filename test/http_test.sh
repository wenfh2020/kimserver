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
    url="$base_url/kim/im/user/"
    request='{"uid":"hello world"}'
    ;;
*)
    echo "invalid param"
    exit 1
    ;;
esac

echo $request
echo ""

curl -v -d "$request" $url | python -m json.tool

#| ascii2uni -a U -q
