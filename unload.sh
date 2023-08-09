#!/bin/bash

RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"



helper(){
    echo "USAGE: unload.sh [OPTION]"
    echo "  -u: umount"
    echo "  -r: rmmod"
    echo "  -a: umount and rmmod"
    return
}

unmount(){
    printf "${RED}unmounting fs${ENDCOLOR}\n"
    sudo umount src/mount
    sudo rm -r src/mount
    return
}

rmmodule(){
    printf "${RED}removing module${ENDCOLOR}\n"
    sudo rmmod singlefilefs
    return
}

while getopts urah: flag
do
    case "${flag}" in
        u) unmount;;
        r) rmmodule;;
        a) unmount
            rmmodule;;
        h) helper;;
        *) printf "${RED}Not a valid option${ENDCOLOR}\n"
            helper;;
    esac
done

if [ $OPTIND -eq 1 ]; 
then 
    printf "${RED}No options were passed, going to perform both umount and rmmod${ENDCOLOR}\n"
    unmount
    rmmodule
fi