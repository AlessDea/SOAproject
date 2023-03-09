#!/bin/bash

repeatChar() {
    local input="$1"
    local count="4096"
    printf -v myString '%*s' "$count"
    printf '%s' "${myString// /$input}"
}


rm tmp.txt
touch tmp.txt

for i in {a..j}; do
	repeatChar $i >> tmp.txt
done

