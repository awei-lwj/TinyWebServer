#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

Log::Log()
{
    m_count = 0;
    m_is_async = false;     // Setting as the sync flag at the beginning
}

Log::~Log()
{
    if( m_fp != NULL )
    {
        fclose(m_fp);
    }
}

bool Log::init(const char* file_name,int close_log, int log_buffer_size,int split_lines,int max_queue_size)
{
    // If the max block size have been setted, using as the async model
    if( max_queue_size >= 1 )
    {
        m_is_async = true;  // Setting the flag

        // Creating the block queue
        m_log_queue = new Block_Queue<std::string>(max_queue_size);
        pthread_t tid;

        // Creating thread to create async file
        pthread_create(&tid, NULL,flush_log_thread,NULL);

    }

    // Cout the length of content
    m_close_log = close_log;
    m_log_buffer_size = log_buffer_size;
    m_buffer = new char[m_log_buffer_size];

    memset(m_buffer, '\0',m_log_buffer_size);

    // The max lines of the log
    m_spilt_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // Finding the first '/' index
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // Defining the name of log
    if( p == NULL )
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,
            my_tm.tm_mon + 1,my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name,p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);

        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name,
            my_tm.tm_year + 1900,my_tm.tm_mon + 1,my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name,"a");

    if( m_fp == NULL )
    {
        return false;
    }

    return true;

}

void Log::write_log(int level, const char *format,...)
{
    struct timeval now = {0,0};

    gettimeofday(&now,NULL);

    time_t t = now.tv_sec;

    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};

    // Log classification
    switch( level )
    {
        case 0:
            strcpy( s, "[debug]: ");
            break;

        case 1:
            strcpy( s, "[info]: ");
            break;

        case 2:
            strcpy( s, "[warning]: ");
            break;

        case 3:
            strcpy( s, "[error]: ");
            break;
        
        default:
            strcpy( s, "[info]: ");
            break;

    }

    m_mutex.lock();

    // Updating the current lines
    m_count++;

    if( m_today != my_tm.tm_mday || m_count % m_spilt_lines == 0 )
    {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        // Formate the timestamp of the log file
        snprintf(tail,16, "%d_%02d_%02d_",my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if ( m_today != my_tm.tm_mday )
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name,m_count / m_spilt_lines);
        }

        m_fp = fopen(new_log,"a");

    }

    m_mutex.unlock();

    va_list valist;

    va_start(valist, format);

    std::string log_str;

    m_mutex.lock();

    // Format: timestamp + content
    // Time format: timestamp
    int n = snprintf(m_buffer, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
            my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // Content format:
    int m = vsnprintf(m_buffer + n, m_log_buffer_size - 1, format, valist);

    m_buffer[ n + m ] = '\n';
    m_buffer[ n + m + 1 ] = '\0';

    log_str = m_buffer;

    m_mutex.unlock();

    // Synchronous, mutex is locked,writing it in the file
    // Asynchronous, put it in the blocking queue
    if( m_is_async && ! m_log_queue->full() )
    {
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }

    va_end(valist);

    return ;

}


void Log::flush(void)
{
    m_mutex.lock();

    // Flushing the write stream buffer
    fflush(m_fp);

    m_mutex.unlock();

    return ;
}




