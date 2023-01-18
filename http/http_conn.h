#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

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
#include <map>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/list_timer.h"
#include "../log/log.h"

class Http_Conn
{
public:
    static const int FILENAME_LEN = 200;

    // The size of buffer in Read and Write
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    Http_Conn() {}
    ~Http_Conn() {}

public:

    // MYSQL
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, 
    string passwd, string sqlname);

    // Closing the connection
    void close_conn(bool real_close = true);

    void process();

    bool read_once(); // Read  all data from the browser connection

    bool write();  // Writing the request data

    sockaddr_in *get_address()
    {
        return &m_address;
    }

    // Synchronization thread initializes database reading table
    void initmysql_result(Connection_Pool *connPool); 

    int timer_flag; // Timer flag
    int improv;


private:
    void init();

    // Read and Write
    HTTP_CODE process_read();    // Reading data from the reader buffer and dealing with request
    bool process_write(HTTP_CODE ret);  // Writing the request data in the writer buffer

    // Main state machine processing message
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();

    char *get_line() { return m_read_buf + m_start_line; };

    // Parsing the part of the request message
    LINE_STATUS parse_line();

    void unmap();

    // Generating 8 corresponding parts based on the format of the request message
    // All of them are calling the do_request method
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_cnt;

    MYSQL *mysql;
    int m_state;  // Read = 0 / Write = 1

private:
    int m_sockfd;      // Socket fd
    sockaddr_in m_address; // Socket address

    // Read
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_index;    // Index of the last pointer in the read buffer
    int m_checked_index;
    int m_start_line;

    // Write
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_index;

    // Main statement machine
    CHECK_STATE m_check_state;
    METHOD m_method;

    // Dealing the request message
    // Storing the name of the read file
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    // File
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_file_iov[2];
    int m_file_iov_cnt;
    int cgi;                   // POST
    char *m_string;            // header data


    int bytes_to_send;
    int bytes_have_sent;

    // LOG
    char *doc_root;   
    int m_TRIGMode;
    int m_close_log;

    // SQL
    map<string, string> m_users;
    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif

