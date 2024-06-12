#!/bin/sh

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]; then
  echo "wrong arguments number"
  exit 1
fi

if [ ! -d $filesdir ]; then
  echo "not a directory"
  exit 1
fi

if [ -z "$searchstr" ]; then
  echo "empty string"
  exit 1
fi

file_n=$(find $filesdir -type f | wc -l)

matching_lines=$(grep -r -o $searchstr $filesdir | wc -l)

echo "The number of files are $file_n and the number of matching lines are $matching_lines"
