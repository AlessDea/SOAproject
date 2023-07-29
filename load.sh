#|/bin/bash

cd ./src
make clean
rm singlefilemakefs 2>/dev/null

make all

# check if the filesystem is already loaded, if it is not then create and load it
MODULE="singlefilefs"
if lsmod | grep -wq "$MODULE"; then
  echo "singlefilefs already loaded"
else
  echo "$MODULE is not mounted! Trying to load it"
  make create-fs 2>/dev/null

  sudo make ins
  echo "module loaded"
fi

# mount the device
sudo make mount-fs

cd ../user

make clear
make user
./user