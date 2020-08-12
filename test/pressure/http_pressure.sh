#!/bin/sh

siege -c 100 -r 100 -H 'Content-Type:application/json' -f ./urls.txt
