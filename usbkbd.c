#include "usbkbd.h"

struct usb_kbd *usb_kbd;
int pipe_irq[2];
int pipe_ack[2];
int pipe_ctl[2];

int *leds_buffer;
sem_t status_lock;
int flag = 0;

pthread_t usb_kbd_irq;
pthread_t usb_kbd_ack;


int main(int argc, char **argv)
{
    int pid;
    int ret;

    leds_buffer = (int *)mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    *leds_buffer = 0;

    ret = pipe(pipe_irq);
    ret = pipe(pipe_ack);
    ret = pipe(pipe_ctl);
    sem_init(&status_lock, 0, 1);

    pid = fork(); // 多进程
    if (pid == 0)
    {
        close(pipe_irq[0]);
        close(pipe_ack[0]);
        close(pipe_ctl[1]);
        pthread_t irq_endpoint;
        pthread_t ctl_endpoint;
        pthread_create(&irq_endpoint, NULL, irq_function, NULL);
        pthread_create(&ctl_endpoint, NULL, ctl_function, NULL);
        pthread_join(irq_endpoint, NULL);
        pthread_join(ctl_endpoint, NULL);
        return ret;
    }
    if (pid > 0)
    {
        // 父进程 3.应用程序将从从主函数调用usb_kbd_open函数开始。
        usb_kbd_open();
    }
    return ret;
}