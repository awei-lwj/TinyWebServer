#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class ThreadPool 
{
public:
    ThreadPool(int actor_model, Connection_Pool *conPool,int threads_number = 8, int max_requests = 1000);
    ~ThreadPool();

    // Adding a task in the requests queue
    bool append(T* request);           

    // Adding a task in the requests queue and Marking the state of this task
    bool append_s(T* request,int state);  

private:

    // The worker function will get the task from the working queue and run it
    static void *worker( void* arg );   
    void run();

private:
    int m_thread_number;           // The number of threads in the threads pool
    int m_max_requests;            // The max number of requests in the working queue

    pthread_t *m_threads;          // Threads[]

    std::list<T *> m_workQueue;    // Working Queue

    sem m_queueState;               // Having the tasks to deal?
    locker m_workQueueLocker;       // Working queue locker

    int m_actor_model;             // Actor model

    Connection_Pool *m_connPool;    // Connection pool
};


// *!* Public function

template<typename T>
ThreadPool<T>::ThreadPool(int actor_model, Connection_Pool *con_pool,int threads_number, int max_requests)
    : m_actor_model( actor_model ),
      m_thread_number(threads_number),
      m_max_requests(max_requests),
      m_threads(NULL),
      m_connPool(con_pool)
      
{
    // Base Case
    if( threads_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    // Initializing the threads ID
    m_threads = new pthread_t[m_thread_number];
    if( !m_threads )
    {
        throw std::exception();
    }

    for( int i = 0; i < threads_number; i++ )
    {
        // Creating the working threads
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 )
        {
            delete[] m_threads;
            throw std::exception();
        }

        // Detaching the working threads and the free threads;
        // Don't delete the working threads
        if( pthread_detach( m_threads[i] ))
        {
            delete[] m_threads;
            throw std::exception();
        }      
        
    }

}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete[] m_threads; 
}

template<typename T>
bool ThreadPool<T>::append(T* request)
{
    m_workQueueLocker.lock();          // Protecting the critical section
    if( m_workQueue.size() >= m_max_requests )
    {
        m_workQueueLocker.unlock();    // Protecting the critical section
        return false;
    }

    m_workQueue.push_back( request );
    m_workQueueLocker.unlock();       // Protecting the critical section
    m_queueState.post(); 

    return true;
}

template<typename T>
bool ThreadPool<T>::append_s(T* request, int state)
{
    m_workQueueLocker.lock();       

    if( m_workQueue.size() >= m_max_requests )
    {
        m_workQueueLocker.unlock();    
        return false;
    }

    request->m_state = state;     // Changing the state of this task
    m_workQueue.push_back( request );
    m_workQueueLocker.unlock();       
    m_queueState.post(); 
    return true;
}

template<typename T>
void* ThreadPool<T>::worker( void* arg )
{
    ThreadPool *pool = (ThreadPool *) arg;
    pool->run();
    return pool;
}

template<typename T>
void ThreadPool<T>::run()
{
    while( true )
    {
        m_queueState.wait();        // Wait semaphore
        m_workQueueLocker.lock();   // locker the semaphore

        if( m_workQueue.empty() )
        {
            m_workQueueLocker.unlock();
            continue;
        }

        // Getting the first task of this working queue and Deleting this task
        T *request = m_workQueue.front();
        m_workQueue.pop_front();
        m_workQueueLocker.unlock();

        if( !request )
        {
            continue;
        }

        if( m_actor_model == 1 )
        {
            if( request->m_state == 0 )
            {
                if( request->read_once() )
                {
                    request->improv = 1;
                    Connection_RAII my_sql_con(&request->mysql,m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if( request->write() )
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            Connection_RAII my_sql_conn(&request->mysql,m_connPool);
            request->process();
        }
    }
}

#endif /* THREADPOOL_H */