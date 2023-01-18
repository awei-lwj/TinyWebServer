#ifndef LIST_TIMER
#define LIST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>

#include "../log/log.h"

class Util_Timer;

struct client_data  // Connection resources
{
    sockaddr_in address;
    int sockfd;
    Util_Timer *timer;
};

class Util_Timer    // Timer Node
{
public:
    Util_Timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;    // The expire time

    void (*cb_func) ( client_data*);

    // Connection resources
    client_data *user_data;

    // Forward timer callback
    Util_Timer *prev;

    // Backward timer callback
    Util_Timer *next;

};

class Sort_Timer_List
{
public:
    Sort_Timer_List();
    ~Sort_Timer_List();

    void add_timer(Util_Timer *timer);

    // When the task is changed, adjusting the timer list
    void adjust_timer(Util_Timer *timer);
    void delete_timer(Util_Timer *timer);

    // Scheduling task processing function
    void trick();


private:

    void add_timer(Util_Timer *timer,Util_Timer *list_head);

    Util_Timer *head;
    Util_Timer *tail;

};

class Utils
{
public:
    Utils() {};
    ~Utils() {};

    void init(int timeslot);

    // Setting the unblocked mode
    int setnonblocking(int fd);

    void add_fd(int epollfd,int fd,bool one_shot,int TRIGMode);

    static void sig_handler( int signal );

    void add_sig(int signal, void(handler)(int), bool restart = true);

    void timer_handler();

    void show_error(int conn_fd,const char *info);

public:
    static int *u_pipe_fd;
    Sort_Timer_List m_timer_list;
    static int u_epoll_fd;
    int m_TIMESLOT;

};

void cb_func(client_data* user_data);

#endif /* LIST_TIMER */