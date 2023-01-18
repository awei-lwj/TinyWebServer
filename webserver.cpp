#include "webserver.h"

WebServer::WebServer()
{
    // Http connection
    users = new Http_Conn[MAX_FD];

    // Root path
    char server_path[200];
    getcwd(server_path,200);
    char root[6] = "/root";
    m_root = (char *) malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // Timer
    users_timer = new client_data[MAX_FD];

}

// Close each connection
WebServer::~WebServer()
{
    close(m_epoll_fd);
    close(m_listen_fd);
    close(m_pipe_fd[1]);
    close(m_pipe_fd[0]);

    delete [] users;
    delete [] users_timer;

    delete m_thread_pool;
}

// Initialize
void WebServer::init(int port, string user, string password,
        string databaseName, int log_write, int opt_linger,
        int trigmode, int sql_num, int thread_num,
        int close_log, int actor_model
    )
{
    m_port = port;
    m_user = user;

    m_password = password;
    m_database = databaseName;

    m_log_write  = log_write;
    m_OPT_LINGER = opt_linger;

    m_TRIGMode = trigmode;
    m_sql_num  = sql_num;

    m_thread_num  = thread_num;
    m_close_log   = close_log;
    m_actor_model = actor_model;

}

// LT / ET model
void WebServer::trig_mode()
{
    // LT + LT
    if( m_TRIGMode == 0 )
    {
        m_LISTENTrigMode = 0;
        m_CONNTrigMode   = 0;
    }
    else if( m_TRIGMode == 1 )   // LT + ET
    {
        m_LISTENTrigMode = 0;
        m_CONNTrigMode   = 1;
    }
    else if( m_TRIGMode == 2 )   // ET + LT
    {
        m_LISTENTrigMode = 1;
        m_CONNTrigMode   = 0;
    }
    else if( m_TRIGMode == 3 )   // ET + ET
    {
        m_LISTENTrigMode = 1;
        m_CONNTrigMode   = 1;
    }

}

// Log Write
void WebServer::log_write()
{
    if( m_close_log == 0 )
    {
        // LOG
        if( m_log_write == 1 )
        {
            Log::get_instance()->init("./ServerLog", m_close_log,2000,800000,800);
        }
        else
        {
            Log::get_instance()->init("./ServerLog", m_close_log,2000,800000,0);
        }
    }
}

// Sql pool
void WebServer::sql_pool()
{
    // Sql connection pool
    m_connection_pool = Connection_Pool::get_Instance();
    m_connection_pool->init("localhost",m_user,m_password,m_database,3306,m_sql_num,m_close_log);

    // Data base write table
    users->initmysql_result(m_connection_pool);
}

// Thread pool
void WebServer::thread_pool()
{
    // Threads pool
    m_thread_pool = new ThreadPool<Http_Conn>(m_actor_model,m_connection_pool,m_thread_num);
}

void WebServer::event_listen()
{
    m_listen_fd = socket(PF_INET, SOCK_STREAM, 0 );
    assert( m_listen_fd >= 0 );

    // Close connection
    if( m_OPT_LINGER == 0 )
    {
        struct linger tmp = {0,1};
        setsockopt( m_listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if( m_OPT_LINGER == 1 )
    {
        struct linger tmp = {1,1};
        setsockopt( m_listen_fd, SOL_SOCKET, SO_LINGER, &tmp,sizeof(tmp) );
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listen_fd, SOL_SOCKET,SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listen_fd, (struct sockaddr *)&address, sizeof(address));
    
    assert( ret >= 0 );
    ret = listen(m_listen_fd, 5);
    assert( ret >= 0 );

    utils.init(TIMESLOT);

    // Creating event in kernel table through epoll
    epoll_event events[MAX_EVENTS];
    m_epoll_fd = epoll_create(5);
    assert( m_epoll_fd != -1 );

    utils.add_fd(m_epoll_fd, m_listen_fd, false, m_LISTENTrigMode);
    Http_Conn::m_epollfd = m_epoll_fd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipe_fd);
    assert(ret != -1);
    utils.setnonblocking(m_pipe_fd[1]);
    utils.add_fd(m_epoll_fd, m_pipe_fd[0], false, 0);

    utils.add_sig(SIGPIPE, SIG_IGN);
    utils.add_sig(SIGALRM, utils.sig_handler, false);
    utils.add_sig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    // Pipe fd/Epoll fd
    Utils::u_pipe_fd  = m_pipe_fd;
    Utils::u_epoll_fd = m_epoll_fd;

}

// Timer
void WebServer::timer(int conn_fd,struct sockaddr_in client_address)
{
    users[conn_fd].init(conn_fd,client_address,m_root, m_CONNTrigMode,
        m_close_log,m_user,m_password,m_database);

    // Initializing client data
    users_timer[conn_fd].address = client_address;
    users_timer[conn_fd].sockfd  = conn_fd;

    // Timer
    Util_Timer *timer = new Util_Timer;
    timer->user_data = &users_timer[conn_fd];
    timer->cb_func   = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[conn_fd].timer = timer;
    utils.m_timer_list.add_timer(timer);
}

// If there has the data transmission, delaying the timer by 3 and adjusting the timer list
void WebServer::adjust_timer(Util_Timer *timer)
{
    time_t cur = time(NULL);

    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_list.add_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

// Dealing timer
void WebServer::deal_timer(Util_Timer *timer,int sock_fd)
{
    timer->cb_func(&users_timer[sock_fd]);
    if( timer )
    {
        utils.m_timer_list.delete_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sock_fd].sockfd);

}

bool WebServer::deal_client_data()
{
    struct sockaddr_in client_address;
    socklen_t client_addr_length = sizeof(client_address);

    if ( m_LISTENTrigMode == 0 )
    {
        int conn_fd = accept(m_listen_fd, (struct sockaddr *)&client_address, &client_addr_length);

        if (conn_fd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }

        if (Http_Conn::m_user_cnt >= MAX_FD)
        {
            utils.show_error(conn_fd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }

        timer(conn_fd, client_address);
    }
    else
    {
        while (1)
        {
            int conn_fd = accept(m_listen_fd, (struct sockaddr *)&client_address, &client_addr_length);

            if (conn_fd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }

            if (Http_Conn::m_user_cnt >= MAX_FD)
            {
                utils.show_error(conn_fd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }

            timer(conn_fd, client_address);
        }
        return false;
    }

    return true;

}

bool WebServer::deal_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];

    ret = recv(m_pipe_fd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }

        }
    }

    return true;
}

void WebServer::deal_read(int sock_fd)
{
    Util_Timer *timer = users_timer[sock_fd].timer;

    //reactor
    if ( m_actor_model == 1 )
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_thread_pool->append_s(users + sock_fd, 0);

        while (true)
        {
            if ( users[sock_fd].improv == 1)
            {
                if ( users[sock_fd].timer_flag == 1)
                {
                    deal_timer(timer, sock_fd);
                    users[sock_fd].timer_flag = 0;
                }

                users[sock_fd].improv = 0;

                break;
            }
        }

    }
    else
    {
        //Proactor model
        if (users[sock_fd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sock_fd].get_address()->sin_addr));

            m_thread_pool->append(users + sock_fd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sock_fd);
        }

    } // end if

}

void WebServer::deal_write(int sock_fd)
{
    Util_Timer *timer = users_timer[sock_fd].timer;
    
    //reactor
    if ( m_actor_model == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_thread_pool->append_s(users + sock_fd, 1);

        while (true)
        {
            if (users[sock_fd].improv == 1)
            {

                if ( users[sock_fd].timer_flag == 1)
                {
                    deal_timer(timer, sock_fd);
                    users[sock_fd].timer_flag = 0;
                }

                users[sock_fd].improv = 0;

                break;
            }
        }

    }
    else
    {
        //proactor
        if (users[sock_fd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sock_fd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }

        }
        else
        {
            deal_timer(timer, sock_fd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout     = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);

        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sock_fd = events[i].data.fd;

            // Dealing with the new client connection
            if (sock_fd == m_listen_fd)
            {
                bool flag = deal_client_data();

                if (false == flag)
                    continue;

            }
            else if ( events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                // Closing connection and Removing timer
                Util_Timer *timer = users_timer[sock_fd].timer;
                deal_timer(timer, sock_fd);

            }
            else if ((sock_fd == m_pipe_fd[0]) && (events[i].events & EPOLLIN)) // Dealing signal
            { 

                bool flag = deal_signal(timeout, stop_server);
                if (flag == false)
                    LOG_ERROR("%s", "deal_client_data failure");

            }
            else if (events[i].events & EPOLLIN)    // Dealing with client data
            {
                deal_read(sock_fd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_write(sock_fd);
            }

        } // end for

        if (timeout)
        {
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }

    } // End while
}
