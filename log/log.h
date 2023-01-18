#ifndef LOG_H
#define LOG_H

/**
* Use the single instance mode to create a log system to record the server's running status, 
* error information and access data. 
* he system can realize the daily classification and super row classification 
* functions, and can use synchronous and asynchronous write methods 
* respectively according to the actual situation.
*/

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>

#include "block_queue.h"

class Log
{
public:

    //  Using the local variables by lazy initialization doesn't need to lock
    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    bool init(const char *file_name,int close_log,int log_buffer_size = 8192,int split_buffer_size = 5000000, int max_queue_size = 0);

    // Sync log writing public method, calling private method async_ write_ log
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return nullptr;
    }

    //Organizing the output content according to the standard format
    void write_log(int level, const char *format,...);

    // Flushing the buffer
    void flush(void);

private:
    Log();
    virtual ~Log();

    // Asynchronous write log
    void *async_write_log()
    {
        std::string single_log;

        // Getting a single log from the blocking queue and writing it to the file
        while( m_log_queue->pop(single_log) )
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }

        return nullptr;
    }

private:
    char dir_name[128]; // name of the directory
    char log_name[128]; // name of the log file

    int m_spilt_lines; // number of lines
    int m_log_buffer_size; // size of log buffer

    long long m_count; // number of the log files lines

    int m_today;

    FILE *m_fp;  // the open file pointer of the log file
    char *m_buffer; // the log buffer

    Block_Queue<std::string> *m_log_queue; // the log queue

    bool m_is_async;     // true if using asynchronous

    locker m_mutex;     // Synchronous lock

    int m_close_log;    // Closing the log file

};



// LOG Judgement
#define LOG_DEBUG(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...)  if(m_close_log == 0) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...)  if(m_close_log == 0) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif /* LOG_H */