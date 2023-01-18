#include "list_timer.h"
#include "../http/http_conn.h"

Sort_Timer_List::Sort_Timer_List()
{
    head = NULL;
    tail = NULL;
}

Sort_Timer_List::~Sort_Timer_List()
{
    Util_Timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void Sort_Timer_List::add_timer(Util_Timer *timer)
{
    if (!timer)
    {
        return;
    }

    if (!head)
    {
        head = tail = timer;
        return;
    }

    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    add_timer(timer, head);
}

void Sort_Timer_List::adjust_timer(Util_Timer *timer)
{
    if (!timer)
    {
        return;
    }

    Util_Timer *temp = timer->next;

    if (!temp || (timer->expire < temp->expire))
    {
        return;
    }

    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        temp->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void Sort_Timer_List::delete_timer(Util_Timer *timer)
{
    if (!timer)
    {
        return;
    }

    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }

    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void Sort_Timer_List::trick()
{
    if (!head)
    {
        return;
    }

    time_t cur = time(NULL);

    Util_Timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }

        tmp->cb_func(tmp->user_data);

        head = tmp->next;

        if (head)
        {
            head->prev = NULL;
        }

        delete tmp;
        tmp = head;
    }
}

void Sort_Timer_List::add_timer(Util_Timer *timer, Util_Timer *list_head)
{
    Util_Timer *prev = list_head;
    Util_Timer *tmp  = prev->next;

    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::add_fd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if ( TRIGMode == 1)    // LT
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else // ET
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    setnonblocking(fd);
}

void Utils::sig_handler(int signal)
{
    int save_errno = errno;
    int message = signal;
    send( u_pipe_fd[1], (char *)&message,1 ,0 );
    errno = save_errno;
}

void Utils::add_sig(int signal, void(handler)(int),bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;

    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }

    sigfillset(&sa.sa_mask);
    assert( sigaction( signal, &sa, NULL ) != -1 );
}

void Utils::timer_handler()
{
    m_timer_list.trick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int conn_fd, const char *info)
{
    send(conn_fd, info, strlen(info),0);
    close(conn_fd);

}

int *Utils::u_pipe_fd = 0;
int Utils::u_epoll_fd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl( Utils::u_epoll_fd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert(user_data);

    close(user_data->sockfd);
    Http_Conn::m_user_cnt--;
}