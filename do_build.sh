export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
make 
make app
aarch64-linux-gnu-g++ devuserapp.cpp -o devuserapp
cp char_driver.ko /mnt/e
cp input0.txt /mnt/e
cp input1.txt /mnt/e
cp input2.txt /mnt/e
cp input3.txt /mnt/e
cp input4.txt /mnt/e
cp devuserapp /mnt/e