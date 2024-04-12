You can use devuserapp.cpp to read a command file to access a specific device and perform I/O. 
To compile it use 

$ make app

To use it, you can use one of input files (input0.txt - input4.txt)

$ ./devuserapp input.txt > output.txt

Note that depending on the size of the ramdisk, devuserapp may generate a 
slightly different output.


