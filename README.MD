# usbkbd_simulator 

## This project directory contains the following contents：

- Makefile
- README.MD
- beyboard.c
- beyboard.h
- input.txt
- input1.txt
- usbkbd.c
- usbkbd.h
- usbkbd_driver_simulator.c
- usbkbd_driver_simulator.h

## Compile the project using the following command：
```
$ make 
```

## If the compilation is not wrong, you can see the generated executable binary in the root of the project
```
$ usbkbd_simulator
```
## Run the project by using the following command：
```
$ ./usbkbd_simulator<input.txt 
```

## Test sample

- input.txt
- input1.txt 

## The terminal prints the following information：
```
$ ./usbkbd_simulator<input.txt 
$ Hello WORLD everyONE!
$ ON OFF ON 

$ ./usbkbd_simulator<input1.txt 
$ Hello
$ ON OFF  

```
## Use the following command to clear the project：
```
$ make clean
```