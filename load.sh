cd ./src
make clean
rm singlefilemakefs 2>/dev/null

make all

make create-fs 2>/dev/null

sudo make ins

sudo make mount-fs

cd ../user

make clear
make user
./user