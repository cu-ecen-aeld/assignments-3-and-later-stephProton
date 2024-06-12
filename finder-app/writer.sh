#!/bin/sh

writefile=$1
writestr=$2

if [ $# -ne 2 ]; then
    echo "wrong arguments: $0 <file> <string_to_write>"
    exit 1
fi

mkdir -p $(dirname $writefile)

echo $writestr > $writefile
