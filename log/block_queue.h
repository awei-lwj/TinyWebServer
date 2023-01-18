#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "../locker/locker.h"

using namespace std;

// Producers and Consumers model
template<class T>
class Block_Queue
{
public:
    Block_Queue(int max_size = 1000)
    {
        if( max_size <= 0 )
        {
            exit(-1);
        }

        // Creating a circular array
        m_max_size = max_size;
        m_array    = new T[max_size];
        m_size     = 0;
        m_front    = -1;
        m_back     = -1;

    }

    void clear()
    {
        m_mutex.lock();

        m_size  = 0;
        m_front = -1;
        m_back  = -1;

        m_mutex.unlock();

    }

    ~Block_Queue()
    {
        m_mutex.lock();
        if( m_array != NULL )
        {
            delete[] m_array;
        }
        m_mutex.unlock();
    }

    bool full()
    {
        m_mutex.lock();
        if( m_size >= m_max_size )
        {
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;

    }

    bool empty()
    {
        m_mutex.lock();
        if( m_size == 0 )
        {
            m_mutex.unlock();
            return true;
        }

        m_mutex.unlock();
        return false;
    }

    // Returning the front element
    bool front(T &value)
    {
        m_mutex.lock();
        if( m_size == 0 )
        {
            m_mutex.unlock();
            return false;
        }

        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // Returning the back element
    bool back(T &value)
    {
        m_mutex.lock();
        if( m_size == 0 )
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    // Returning the size of the queue
    int size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_size;

        m_mutex.unlock();

        return tmp;
    }

    // Returning the maximum size of the queue
    int max_size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();

        return tmp;
    }


    // Adding the element in the queue
    // Producing and Creating model
    bool push(const T &item)
    {
        m_mutex.lock();
        if( m_size >= m_max_size )
        { 
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = ( m_back + 1 ) % m_max_size;
        m_array[m_back] = item;

        m_size++;
        
        m_cond.broadcast();
        m_mutex.unlock();

        return true;
    }

    // Popping the element
    // Producer and Consumer model
    bool pop(T &item)
    {
        m_mutex.lock();
        while( m_size <= 0 )
        {
            if( !m_cond.wait( m_mutex.get() ) )
            {
                m_mutex.unlock();
                return false;
            }

        }

        m_front = ( m_front + 1 ) % m_max_size;
        item = m_array[m_front];
        m_size--;

        m_mutex.unlock();
        return true;
    }

    // Adding the function of timeout handler
    bool pop(T &item, int ms_timeout)
    {
        struct timespec  t  = {0,0};
        struct timeval  now = {0,0};

        gettimeofday(&now, NULL);
        
        m_mutex.lock();
        if( m_size <= 0 )
        {
            t.tv_sec  = now.tv_sec + ms_timeout/1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;

            if( !m_cond.timewait( m_mutex.get(),t ) )
            {
                m_mutex.unlock();
                return false;
            }
        }

        if( m_size <= 0 )
        {
            m_mutex.unlock();
            return false;
        }

        m_front = ( m_front + 1 ) % m_max_size;
        item = m_array[m_front];
        m_size--;

        m_mutex.unlock();
        return true;

    }
    
private:
    locker m_mutex;       // Locker
    cond m_cond;          // Condition semaphore

    T *m_array;

    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
    

};

#endif /* BLOCK_QUEUE_H */