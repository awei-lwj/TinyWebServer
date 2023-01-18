#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/errno.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD     = 65536;      // Maximum number of file descriptors
const int MAX_EVENTS = 10000;        // Maximum number of events
const int TIMESLOT   = 5;          // The max out timestamp


class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string password,
        string databaseName, int log_write, int opt_linger,
        int trigmode, int sql_num, int thread_num,
        int close_log, int actor_model
    );

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();

    // Event
    void event_listen();
    void eventLoop();

    // Timer
    void timer( int conn_fd, struct sockaddr_in client_address);
    void adjust_timer(Util_Timer *timer);
    void deal_timer(Util_Timer *timer,int sock_fd);

    // Client
    bool deal_client_data();
    bool deal_signal(bool& timeout, bool& stop_server);

    // Read and write
    void deal_read(int socket_fd);
    void deal_write(int socket_fd);

public:
    
    // Basic elements
    int m_port;         // The port
    char *m_root;       // The root
    int m_log_write;    // The flag of writing in the log
    int m_close_log;    // The flag of closing the log
    int m_actor_model;  // The actor model

    // Epoll
    int m_pipe_fd[2];     // The epoll fd
    int m_epoll_fd;         // The epoll fd
    Http_Conn *users;

    // Data base
    Connection_Pool *m_connection_pool; // The connection pool
    string m_user;        // The user
    string m_password;    // The password
    string m_database;    // The database
    int m_sql_num;       // The number of sql

    // Thread pool
    ThreadPool<Http_Conn> *m_thread_pool; // The thread pool
    int m_thread_num;  // The number of threads

    // Epoll Events
    epoll_event events[MAX_EVENTS]; 

    int m_listen_fd;
    int m_OPT_LINGER; 
    int m_TRIGMode;
    int m_LISTENTrigMode;
    int m_CONNTrigMode;

    // Timer
    struct client_data *users_timer;
    Utils utils;

};



#endif /* WEBSERVER_H*/
