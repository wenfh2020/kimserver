#!/bin/sh

siege -c 200 -r 200 -H 'Content-Type:application/json' -f ./urls.txt
