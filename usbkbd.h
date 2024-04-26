
#ifndef __USBKBD__
#define __USBKBD__


#include "stdio.h"
#include "sys/mman.h"
#include <semaphore.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "beyboard.h"
#include "usbkbd_driver_simulator.h"


typedef void *(*handler)(void *urb);
struct usb_urb
{
    char *data_buf;
    int fd_pipe;
    char *prev;
    handler handle;
};

struct usb_kbd
{
    struct usb_urb *irq_urb;
    struct usb_urb *led_urb;
    sem_t submit_led;
    sem_t led_urb_lock;
    bool irq_urb_submitted;
    bool led_urb_submitted;
    int caps_mode;
    int pending_led_cmd;
};



#endif
