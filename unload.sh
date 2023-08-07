#!/bin/bash

RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"



function helper(){
    echo "USAGE: unload.sh [OPTION]"
    echo "  -r: umount"
    echo "  -u: rmmod"
    echo "  -a: umount and rmmod"
    return
}

function unmount(){
    echo -e "${RED}unmounting fs${ENDCOLOR}"
    sudo umount src/mount
    sudo rm -r src/mount
    return
}

function rmmodule(){
    echo -e "${RED}removing module${ENDCOLOR}"
    sudo rmmod singlefilefs
    sudo rm -r src/mount
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
        *) echo -e "${RED}Not a valid option${ENDCOLOR}"
            helper;;
    esac
done

if [ $OPTIND -eq 1 ]; 
then 
    echo -e "${RED}No options were passed, going to perform both umount and rmmod${ENDCOLOR}"
    umount
    rmmodule
fi