all:usbkbd.o beyboard.o usbkbd_driver_simulator.o
	gcc usbkbd.o beyboard.o usbkbd_driver_simulator.o -pthread -o usbkbd_simulator
	
clean:
	rm *.o usbkbd_simulator