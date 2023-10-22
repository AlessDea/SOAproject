#!/bin/bash
RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"
BOLDGREEN="\e[1;${GREEN}"
BOLDRED="\e[1;${RED}"
FILE="image"
MODULE="singlefilefs"
create_d=true


cd ./src || exit
make clean
make all # 2>/dev/null

# check if the device already exists
echo "${FILE}"
if test -f $FILE; 
then

  printf "A device already exists.\n"
  read -p "Do you want to use it? (y/n) " yn

  case $yn in
    [yY] )  echo "using it..."
            create_d=false;;
    [nN] )  echo "creating a new device..."
            create_d=true;;
    * )     echo "invalid response"
            exit 1;;
  esac

else

  create_d=true

fi

# check if the singlefilefs module is already loaded, if it is not then create and load it
if lsmod | grep -wq "$MODULE"; then
  printf "${BOLDGREEN}$MODULE already loaded${ENDCOLOR}\n"

  if [ $create_d = true ]; then
    make create-fs 2>/dev/null
  fi

else
  # if the file system is not loaded then the device driver has to be created
  printf "${BOLDRED} $MODULE is not loaded! Trying to load it${ENDCOLOR}\n"

  sudo make ins
  printf "${BOLDGREEN} module correctly loaded${ENDCOLOR}\n"

  if [ $create_d = true ]; then
    make create-fs 2>/dev/null
  fi
fi

# mount the device
sudo make mount-fs

# simple user client
# read -p "Do you want to run user client? (y/n)" yn
# case $yn in 
#  [yY] )  chmod u+x usr.sh
#          ./usr.sh;;
#  [nN] )  echo "you can run it with the script usr.sh"
#          exit 0;;
#  * )     echo "invalid response"
#          exit 1;;
#esac
