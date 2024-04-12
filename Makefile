
obj-m := char_driver.o

KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) modules
	
app: 
	g++ -o devuserapp devuserapp.cpp -std=c++11

clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order devuserapp
