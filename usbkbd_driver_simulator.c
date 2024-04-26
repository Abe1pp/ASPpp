#include "usbkbd.h"

extern struct usb_kbd *usb_kbd;
extern int pipe_irq[2];
extern int pipe_ack[2];
extern int pipe_ctl[2];

extern int *leds_buffer;
extern sem_t status_lock;
extern int flag;

extern pthread_t usb_kbd_irq;
extern pthread_t usb_kbd_ack;

void *ack_poll_func(void *irq_submit_urb)
{
    struct usb_urb *led_submit_urb_temp = (struct usb_urb *)irq_submit_urb;
    char c = 'C';
    char *alert = &c;
    while (1)
    {
        sem_wait(&(usb_kbd->submit_led));
        write(pipe_ctl[1], alert, 1);
        pthread_t led_submit_urb_thread;
        if (read(led_submit_urb_temp->fd_pipe, led_submit_urb_temp->data_buf, 1) > 0)
        {
            pthread_create(&led_submit_urb_thread, NULL, led_submit_urb_temp->handle, (void *)irq_submit_urb);
            pthread_join(led_submit_urb_thread, NULL);
        }
    }
    close(pipe_ctl[1]);
    close(led_submit_urb_temp->fd_pipe);
    return 0;
}

void *irq_poll_func(void *irq_submit_urb)
{
    struct usb_urb *irq_submit_urb_temp = (struct usb_urb *)irq_submit_urb;

    pthread_t irq_submit_urb_thread;
    while (read(irq_submit_urb_temp->fd_pipe, irq_submit_urb_temp->data_buf, 1) > 0)
    {
        pthread_create(&irq_submit_urb_thread, NULL, irq_submit_urb_temp->handle, (void *)irq_submit_urb);
        pthread_join(irq_submit_urb_thread, NULL);
    }
    printf("\n");
    sleep(0.5);
    exit(0);
}

int usb_submit_urb(struct usb_urb *irq_submit_urb, struct usb_urb *led_submit_urb)
{

    // 每个URB完成处理程序的每次执行都将通过创建一个新的线程来实现。
    sem_wait(&status_lock);
    if (flag == 0)
    {
        flag = 1;
        sem_post(&status_lock);
        pthread_create(&usb_kbd_irq, NULL, irq_poll_func, (void *)irq_submit_urb);
        pthread_create(&usb_kbd_ack, NULL, ack_poll_func, (void *)led_submit_urb);
        pthread_join(usb_kbd_irq, NULL);
        pthread_join(usb_kbd_ack, NULL);
        exit(0);
    }
    sem_post(&status_lock);
    sem_wait(&usb_kbd->led_urb_lock);
    usb_kbd->led_urb_submitted = true;
    sem_post(&usb_kbd->led_urb_lock);
    sem_post(&usb_kbd->submit_led);
    return 0;
}

void *led_handler(void *led_urb)
{
    sem_wait(&usb_kbd->led_urb_lock);
    if (usb_kbd->pending_led_cmd == 0)
    {
        usb_kbd->led_urb_submitted = false;
        sem_post(&usb_kbd->led_urb_lock);
    }
    else
    {
        *leds_buffer = (*leds_buffer + 1) % 2;
        usb_kbd->pending_led_cmd--;
        sem_post(&usb_kbd->led_urb_lock);
        usb_submit_urb(NULL, usb_kbd->led_urb);
    }
    return 0;
}

void *usb_kbd_event()
{
    sem_wait(&usb_kbd->led_urb_lock);
    if (usb_kbd->led_urb_submitted == true)
    {
        usb_kbd->pending_led_cmd += 1;
        sem_post(&usb_kbd->led_urb_lock);
    }
    *leds_buffer = (*leds_buffer + 1) % 2;
    sem_post(&usb_kbd->led_urb_lock);
    usb_submit_urb(NULL, usb_kbd->led_urb);
}

void input_report_key(char key)
{
    if (isalnum(key))
    {
        if (*(usb_kbd->irq_urb->prev) == '@')
        {
            printf("%c", *usb_kbd->irq_urb->prev);
            *usb_kbd->irq_urb->prev = '\0';
        }
        if (usb_kbd->caps_mode == 1)
        {
            printf("%c", toupper(key));
            return;
        }
        printf("%c", key);
        return;
    }

    if (key == '#') // 没有按键事件
    {
        if (*usb_kbd->irq_urb->prev == '@')
        {
            printf("%c", *usb_kbd->irq_urb->prev);
            usb_kbd->irq_urb->prev = '\0';
        }
        return;
    }

    if (key == '@') // CAPSLOCK press
    {
        if (*usb_kbd->irq_urb->prev == '@')
        {
            printf("%c", *usb_kbd->irq_urb->prev);
            usb_kbd->irq_urb->prev = "\0";
        }
        *usb_kbd->irq_urb->prev = '@';
        return;
    }

    if (key == '&') // CAPSLOCK release
    {
        if (*usb_kbd->irq_urb->prev == '@')
        {
            usb_kbd->caps_mode = (usb_kbd->caps_mode + 1) % 2;
            *usb_kbd->irq_urb->prev = '\0';
            pthread_t usb_event;
            pthread_create(&usb_event, NULL, usb_kbd_event, NULL);
            return;
        }
        printf("%c\n", key);
    }
    else
    {
        if (*usb_kbd->irq_urb->prev == '@')
        {
            printf("%c", *usb_kbd->irq_urb->prev);
            usb_kbd->irq_urb->prev = "\0";
        }
        printf("%c", key);
        return;
    }
    return;
}

void *irq_handler(void *irq_urb)
{
    struct usb_urb *irq_urb_tmep = (struct usb_urb *)irq_urb;

    input_report_key(*irq_urb_tmep->data_buf);
    return 0;
}

int usb_urb_init(struct usb_urb *irq, void *handler, int pipe)
{
    irq->data_buf = (char *)malloc(sizeof(char));
    irq->prev = (char *)malloc(sizeof(char));
    irq->handle = handler;
    irq->fd_pipe = pipe;
    return 0;
}

int usb_kbd_init(struct usb_kbd *usb_kbd)
{
    int ret;
    usb_kbd->irq_urb = (struct usb_urb *)malloc(sizeof(struct usb_urb));
    usb_kbd->led_urb = (struct usb_urb *)malloc(sizeof(struct usb_urb));

    ret = usb_urb_init(usb_kbd->irq_urb, irq_handler, pipe_irq[0]);
    ret = usb_urb_init(usb_kbd->led_urb, led_handler, pipe_ack[0]);

    usb_kbd->caps_mode = 0;
    usb_kbd->pending_led_cmd = 0;
    sem_init(&(usb_kbd->led_urb_lock), 0, 1);
    usb_kbd->irq_urb_submitted = true;
    usb_kbd->led_urb_submitted = false;
    sem_init(&(usb_kbd->submit_led), 0, 0);
    return ret;
}

int usb_kbd_open()
{
    close(pipe_irq[1]);
    close(pipe_ack[1]);
    close(pipe_ctl[0]);

    usb_kbd = (struct usb_kbd *)malloc(sizeof(struct usb_kbd));
    usb_kbd_init(usb_kbd);
    usb_submit_urb(usb_kbd->irq_urb, usb_kbd->led_urb);
    return 0;
}