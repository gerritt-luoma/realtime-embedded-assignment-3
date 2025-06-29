#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

#define SNDRCV_MQ "/send_receive_mq"
#define MAX_MSG_SIZE 128
#define ERROR (-1)

struct mq_attr mq_attr;

static char canned_msg[] = "this is a test, and only a test, in the event of a real emergency, you would be instructed ...";

void* sender(void* arg)
{
    mqd_t mymq;
    int nbytes;

    mymq = mq_open(SNDRCV_MQ, O_CREAT | O_RDWR, S_IRWXU, &mq_attr);
    if (mymq < 0) {
        perror("sender mq_open");
        pthread_exit(NULL);
    } else {
        printf("sender opened mq\n");
    }

    if ((nbytes = mq_send(mymq, canned_msg, sizeof(canned_msg), 30)) == ERROR) {
        perror("mq_send");
    } else {
        printf("send: message successfully sent\n");
    }

    mq_close(mymq);
    pthread_exit(NULL);
}

void* receiver(void* arg)
{
    mqd_t mymq;
    char buffer[MAX_MSG_SIZE];
    unsigned int prio;
    int nbytes;

    mymq = mq_open(SNDRCV_MQ, O_CREAT | O_RDWR, S_IRWXU, &mq_attr);
    if (mymq == (mqd_t)ERROR) {
        perror("receiver mq_open");
        pthread_exit(NULL);
    }

    if ((nbytes = mq_receive(mymq, buffer, MAX_MSG_SIZE, &prio)) == ERROR) {
        perror("mq_receive");
    } else {
        buffer[nbytes] = '\0';
        printf("receive: msg \"%s\" received with priority = %d, length = %d\n",
               buffer, prio, nbytes);
    }

    mq_close(mymq);
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t sender_thread, receiver_thread;
    pthread_attr_t attr_sender, attr_receiver;
    struct sched_param param_sender, param_receiver;

    // Set up message queue attributes
    mq_attr.mq_flags = 0;
    mq_attr.mq_maxmsg = 10;
    mq_attr.mq_msgsize = MAX_MSG_SIZE;
    mq_attr.mq_curmsgs = 0;

    int rt_max_prio = sched_get_priority_max(SCHED_FIFO);

    // Initialize attributes and set SCHED_FIFO for sender
    pthread_attr_init(&attr_sender);
    pthread_attr_setinheritsched(&attr_sender, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr_sender, SCHED_FIFO);
    param_sender.sched_priority = rt_max_prio - 1;
    pthread_attr_setschedparam(&attr_sender, &param_sender);

    // Initialize attributes and set SCHED_FIFO for receiver
    pthread_attr_init(&attr_receiver);
    pthread_attr_setinheritsched(&attr_receiver, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr_receiver, SCHED_FIFO);
    param_receiver.sched_priority = rt_max_prio - 2;
    pthread_attr_setschedparam(&attr_receiver, &param_receiver);

    // Create sender and receiver threads
    if (pthread_create(&sender_thread, &attr_sender, sender, NULL) != 0) {
        perror("pthread_create sender");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&receiver_thread, &attr_receiver, receiver, NULL) != 0) {
        perror("pthread_create receiver");
        exit(EXIT_FAILURE);
    }

    // Wait for both threads to complete
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    return 0;
}