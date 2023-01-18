#include <iostream>
#include <chrono>
#include <cstdlib>
#include <pthread.h>
#include <time.h>

#include "../skiplist/skiplist.h"

// *!* Testing the Key-Value storage basing on the SkipList

#define NUM_THREADS 1
#define TEST_COUNT 1000000


SkipList<int, std::string> skipList(18);

void *insertElement(void* threadId)
{
    long tid;

    tid = (long)threadId;

    std::cout << tid << std::endl;
    int tmp = TEST_COUNT / NUM_THREADS;

    for( int i = tid * tmp, count = 0; count < tmp;i++ )
    {
        count++;
        skipList.insert_element( rand() % TEST_COUNT, "awei" ); 
    }   

    pthread_exit(NULL);

}

void *getElement(void* threadId)
{
    long tid;
    tid = (long)threadId;
    std::cout << tid << std::endl;
    int tmp = TEST_COUNT / NUM_THREADS;

    for( int i = tid * tmp, count = 0; count < tmp;i++)
    {
        count++;
        skipList.search_element( rand() % TEST_COUNT );
    }

    pthread_exit(NULL);
}

int main()
{
    
    srand( time(NULL) );

    // Insert Element operation tests
    {
        pthread_t threads[NUM_THREADS];
        int rc;
        int i;

        auto start = std::chrono::high_resolution_clock::now();

        for( int i = 0; i < NUM_THREADS;i++ )
        {
            std::cout << "main() : creating thread" << i << std::endl;
            rc = pthread_create( &threads[i], NULL, insertElement, (void *)i);

            if( rc )
            {
                std::cout << "Error : Unable to create thread, " << rc << std::endl;
                exit(-1);
            }

        }

        void *ret;
        for( i = 0; i < NUM_THREADS;i++ )
        {
            if( pthread_join(threads[i], &ret) != 0 )
            {
                perror("pthread_create() error");
                exit(3);
            }
        }

        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        std::cout << "Insert elapsed: " << elapsed.count() << std::endl;
    }
    

 
    // Search Element operation tests

    // skipList.display_list();

    // Get Element operation tests
    {
        pthread_t threads[NUM_THREADS];
        int rc;
        int i;

        auto start = std::chrono::high_resolution_clock::now();

        for( int i = 0; i < NUM_THREADS;i++ )
        {
            std::cout << "main() : creating thread" << i << std::endl;
            rc = pthread_create( &threads[i], NULL, getElement, (void *)i);

            if( rc )
            {
                std::cout << "Error : Unable to create thread, " << rc << std::endl;
                exit(-1);
            }

        }

        void *ret;
        for( i = 0; i < NUM_THREADS;i++ )
        {
            if( pthread_join(threads[i], &ret) != 0 )
            {
                perror("pthread_create() error");
                exit(3);
            }
        }

        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        std::cout << "Get elapsed: " << elapsed.count() << std::endl;
    }

    pthread_exit(NULL);


    return 0;

}