#!/bin/sh

siege -c 300 -r 1000 -H 'Content-Type:application/json' -f ./urls.txt
