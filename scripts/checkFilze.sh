#!/bin/bash

# use nullglob in case there are no matching files
shopt -s nullglob

# create an array with all the filer/dir inside ~/myDir
arr=($1/*)

# iterate through array using a counter
for ((i=0; i<${#arr[@]}; i++)); do
    if [ -f "${arr[$i]}1" ]; then
        cmp --silent "${arr[$i]}1" "${arr[$i]}" && echo "${arr[$i]}:is Identical!" || echo "ERROR: ${arr[$i]} is Different! ###"
    fi
done


