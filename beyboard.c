

#include "usbkbd.h"


extern int pipe_irq[2];
extern int pipe_ack[2];
extern int pipe_ctl[2];

extern int *leds_buffer;
extern sem_t status_lock;
extern int flag ;

void *irq_function()
{;
    int fd =  open("001.txt",O_RDONLY);
    char *buf = (char *)malloc(sizeof(char));
    while (read(STDIN_FILENO, buf, 1) > 0)
    {
        write(pipe_irq[1], buf, 1);
        sleep(0.5);
    }
    close(pipe_irq[1]);
    close(fd);
    return 0;
}

void *ctl_function()
{

    char *led_buf = (char *)malloc(sizeof(char));
    char *bufr = (char *)malloc(sizeof(char));
    char bufw = 'A';
    int count = 1;
    while (read(pipe_ctl[0], bufr, 1) > 0)
    {
        led_buf = (char *)realloc(led_buf, count * sizeof(char));
        if (*leds_buffer == 0)
            led_buf[count - 1] = '0';
        else
            led_buf[count - 1] = '1';
        write(pipe_ack[1], &bufw, 1);
        count++;
    }
    close(pipe_ack[1]);
    close(pipe_ctl[0]);
    for (int i = 0; i < count - 1; i++)
        if (led_buf[i] == '0')
            printf("OFF ");
        else
            printf("ON ");

    printf("\n");
    exit(0);
}


