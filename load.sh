#!/bin/bash
RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"
BOLDGREEN="\e[1;${GREEN}m"
BOLDRED="\e[1;${RED}m"
FILE="/src/image"
MODULE="singlefilefs"
create_d=true


cd ./src || exit
make clean
make all # 2>/dev/null

# check if the device already exists
if test -f $FILE; 
then

  echo "A device already exists."
  read -p -r "Do you want to use it? (y/n) " yn

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
  echo -e "${BOLDGREEN}$MODULE already loaded${ENDCOLOR}"

  if [ $create_d = true ]; then
    make create-fs 2>/dev/null
  fi

else
  # if the file system is not loaded then the device driver has to be created
  echo -e "${BOLDRED}$MODULE is not loaded! Trying to load it${ENDCOLOR}"

  sudo make ins
  echo -e "${BOLDGREEN}module correctly loaded${ENDCOLOR}"

  if [ $create_d = true ]; then
    make create-fs 2>/dev/null
  fi
fi

# mount the device
sudo make mount-fs

# simple user client
read -p -r "Do you want to run user client? (y/n) " yn
case $yn in 
  [yY] )  sh user.sh;;
  [nN] )  echo "you can run it with the script usr.sh"
          exit 0;;
  * )     echo "invalid response"
          exit 1;;
esac
