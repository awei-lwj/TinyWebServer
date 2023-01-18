#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

// Define the the statement of the http response
const char *ok_200_title    = "OK";

const char *error_400_title = "Bad Request";
const char *error_400_form  = "Your request has bad syntax or is inherently impossible to satisfy.\n";

const char *error_403_title = "Forbidden";
const char *error_403_form  = "You do not have permission to get file form this server.\n";

const char *error_404_title = "Not Found";
const char *error_404_form  = "The requested file was not found on this server.\n";

const char *error_500_title = "Internal Error";
const char *error_500_form  = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;

// *!* In this project, the epoll functions are including : \
// *!*Non-blocking, Register Events, Delete Events, Modify EPOLLONESHOT

// *!* The http connection is about Four parts:
// *!* 1. HTTP request receive
// *!* 2. Main/Slave statement machine to parse the HTTP request
// *!* 3. Generating the HTTP response
// *!* 4. MySql

// *!* HTTP request receive
// Setting the file descriptor as the unblocking model
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// Registering the kernel event table with read events in ET mode, and select EPOLLONESHO
void add_fd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == 1)   // ET 
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else                // LT
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
       

    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
        
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// Deleting the file descriptor from the kernel time table
void remove_fd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// Setting the event flags as the EPOLLONESHOT flag
void mod_fd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if ( TRIGMode == 1)   // ET
    {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }
    else                 // LT
    {                
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
        
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);

}

// *!* Initialing the sql server
void Http_Conn::initmysql_result(Connection_Pool *connPool)
{
    // Getting a free connection from the pool
    MYSQL *mysql = NULL;
    Connection_RAII mysqlcon(&mysql, connPool);    // mysql connection

    // Username and password
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // Result
    MYSQL_RES *result = mysql_store_result(mysql);

    // The lines of the result
    int num_fields = mysql_num_fields(result);

    // the fields of the result
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // Insert it in the map
    // TODO: Using the skiplist to storage the user and password
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string tmp1(row[0]);
        string tmp2(row[1]);
        users[tmp1] = tmp2;
    }
}

int Http_Conn::m_user_cnt = 0;
int Http_Conn::m_epollfd  = -1;

// Initializing the connection and Setting the check_state as default analyze requests line status
void Http_Conn::init()
{
    mysql = NULL;
    m_check_state = CHECK_STATE_REQUESTLINE;
    cgi = 0;        
    m_state    = 0;     // Read status

    // Timer
    timer_flag = 0;
    improv     = 0;

    bytes_to_send   = 0;
    bytes_have_sent = 0;

    // HTTP
    m_linger  = false;
    m_method  = GET;
    m_url     = 0;
    m_version = 0;
    m_content_length = 0;
    m_host    = 0;

    // Read and Write
    m_start_line    = 0;
    m_checked_index = 0;
    m_read_index    = 0;
    m_write_index   = 0;

    // Buffer and File
    memset(m_read_buf , '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// Initializing the connection
void Http_Conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd  = sockfd;
    m_address = addr;

    add_fd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_cnt++;

    // Connection reset
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user  , user.c_str());       // user name
    strcpy(sql_passwd, passwd.c_str());   // password
    strcpy(sql_name  , sqlname.c_str());    // database

    init();

}


// Closing a connection. the user count - 1;
void Http_Conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        remove_fd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_cnt--;
    }
}

// *!* Slave status machine
// Dealing with a line data and Returning the status code of line data
// Returning Line ok,line bad,line open
Http_Conn::LINE_STATUS Http_Conn::parse_line()
{
    char temp;
    for (; m_checked_index < m_read_index; ++m_checked_index)
    {
        temp = m_read_buf[m_checked_index]; // Dealing with a line

        if (temp == '\r')
        {
            if ((m_checked_index + 1) == m_read_index)
            {
                 return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_index + 1] == '\n')
            {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }

            // Error grammar parsing
            return LINE_BAD;
        }
        else if (temp == '\n') // Getting the complete line
        {
            // If the last character is '\r', meaning that receiving the complete line
            if (m_checked_index > 1 && m_read_buf[m_checked_index - 1] == '\r') 
            {
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++]   = '\0';
                return LINE_OK;
            }

            // Error grammar
            return LINE_BAD;
        }

    }

    return LINE_OPEN;
}

// Reading customer data circularly until no data is readable or the other party closes the connection
bool Http_Conn::read_once()
{
    if (m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;

    // LT
    if (m_TRIGMode == 0)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        m_read_index += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    else // ET
    {
        while (true)
        {
            // Reading the data from the socket and storing it in the m_read_buffer
            bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);

            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                    
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }

            m_read_index += bytes_read;
        }

        return true;

    }

}

// *!* Main statement machine function
// Parsing the http request line,getting the method,url and http version
Http_Conn::HTTP_CODE Http_Conn::parse_request_line(char *text)
{
    // In the HTTP message, the request line is used to specify the request type, \
    // the resources to be accessed, and the HTTP version used. \
    // Each part is separated by t or a space.

    // Getting the url
    m_url = strpbrk(text, " \t");

    // Error grammar
    if (!m_url)
    {
        return BAD_REQUEST;
    }

    *m_url++ = '\0';

    // Getting the data to compare Get and Post methods for ensuring the method
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        // Error grammar
        return BAD_REQUEST;
    }

    // Passing the space and '\t'
    m_url += strspn(m_url, " \t");

    // Getting the version
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        // Error grammar
        return BAD_REQUEST;
    }

    // Passing the space and '\t'
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    // Only supporting HTTP 1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        // Error grammar
        return BAD_REQUEST;
    }

    // Dealing with "http://" and "https://"
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
        
    // Showing the judge.html when the m_url is '/'
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "judge.html");
    }
        
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// Parsing the header line of HTTP request
Http_Conn::HTTP_CODE Http_Conn::parse_headers(char *text)
{
    // Judging null or Get and Post headers
    if (text[0] == '\0')
    {
        // Get or Post headers
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) // Parsing the connection part of headers
    {
        text += 11;
        text += strspn(text, " \t");

        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0) // Content-Length
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)   // Host
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else  // Error
    {
        LOG_INFO("oop!unknow header: %s", text);
    }


    return NO_REQUEST;
}

// parse contents
// Judging the HTTP request content completion
Http_Conn::HTTP_CODE Http_Conn::parse_content(char *text)
{
    if (m_read_index >= (m_content_length + m_checked_index))
    {
        text[m_content_length] = '\0';

        // The last part of the Post request is Username and password
        m_string = text;

        return GET_REQUEST;
    }

    return NO_REQUEST;
}

// *!* Server thread
Http_Conn::HTTP_CODE Http_Conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_index;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

Http_Conn::HTTP_CODE Http_Conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/awei.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
void Http_Conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool Http_Conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_file_iov, m_file_iov_cnt);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                mod_fd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            
            unmap();
            return false;
        }

        bytes_have_sent += temp;
        bytes_to_send -= temp;

        if (bytes_have_sent >= m_file_iov[0].iov_len)
        {
            m_file_iov[0].iov_len  = 0;
            m_file_iov[1].iov_base = m_file_address + (bytes_have_sent - m_write_index);
            m_file_iov[1].iov_len  = bytes_to_send;
        }
        else
        {
            m_file_iov[0].iov_base = m_write_buf + bytes_have_sent;
            m_file_iov[0].iov_len  = m_file_iov[0].iov_len - bytes_have_sent;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

// *!*  Generating 8 corresponding parts based on the format of the request message
bool Http_Conn::add_response(const char *format, ...)
{
    if (m_write_index >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;
    va_start(arg_list, format);

    int len = vsnprintf(m_write_buf + m_write_index, WRITE_BUFFER_SIZE - 1 - m_write_index, format, arg_list);
    
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_index))
    {
        va_end(arg_list);
        return false;
    }

    m_write_index += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

bool Http_Conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool Http_Conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}

bool Http_Conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool Http_Conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool Http_Conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool Http_Conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool Http_Conn::add_content(const char *content)
{
    return add_response("%s", content);
}

bool Http_Conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));

        if (!add_content(error_500_form))
        {
            return false;
        }
            
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));

        if (!add_content(error_404_form))
        {
            return false;
        }
            
        break;
    }

    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));

        if (!add_content(error_403_form))
        {
            return false;
        }
            
        break;
    }

    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);

            m_file_iov[0].iov_base = m_write_buf;
            m_file_iov[0].iov_len  = m_write_index;
            m_file_iov[1].iov_base = m_file_address;
            m_file_iov[1].iov_len  = m_file_stat.st_size;
            m_file_iov_cnt = 2;

            bytes_to_send = m_write_index + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }

    }

    default:
        return false;
    }

    m_file_iov[0].iov_base = m_write_buf;
    m_file_iov[0].iov_len  = m_write_index;
    m_file_iov_cnt         = 1;

    bytes_to_send = m_write_index;
    return true;
}


void Http_Conn::process()
{
    HTTP_CODE read_ret = process_read();

    if (read_ret == NO_REQUEST)
    {
        mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }

    bool write_ret = process_write(read_ret);

    if (!write_ret)
    {
        close_conn();
    }

    mod_fd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
