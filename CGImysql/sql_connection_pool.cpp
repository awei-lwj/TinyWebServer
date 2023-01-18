#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>

#include "sql_connection_pool.h"

using namespace std;

Connection_Pool::Connection_Pool()
{
    m_CurrentConnections = 0;
    m_FreeConnections = 0;
}

Connection_Pool::~Connection_Pool()
{
    DestroyPool();
}

// RAII
Connection_Pool *Connection_Pool::get_Instance()
{
    static Connection_Pool connection_pool;
    return &connection_pool;
}

// Initialize the connection pool
void Connection_Pool::init(string url,string user,string password,
        string DataBaseName,int port,int maxConnections,int close_log)
{
    // Initializing the Data Base
    m_url          = url;
    m_user         = user;
    m_password     = password;
    m_DataBaseName = DataBaseName;
    m_port         = port;
    m_close_log    = close_log; 

    // Creating the max connection of sql
    for( int i = 0; i < maxConnections; i++ )
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if( con == NULL )
        {
            LOG_ERROR("MySQL Error: Unable to create connection");
            exit(1);
        }

        con = mysql_real_connect(con, url.c_str(),user.c_str(),password.c_str(),
            DataBaseName.c_str(),port,NULL,0);

        if( con == NULL )
        {
            LOG_ERROR("MySQL Error: Unable to create connection");
            exit(1);
        }

        // Updating the connection pool
        connection_list.push_back(con);
        m_FreeConnections++;
    }

    // Updating the max connections
    reserve = sem(m_FreeConnections);
    m_MaxConnections = m_FreeConnections;
    

}

// Getting a free connection from the pool
MYSQL *Connection_Pool::GetConnection()
{
    MYSQL *con = NULL;

    if( connection_list.size() == 0 )
    {
        return NULL;
    }

    reserve.wait();

    lock.lock();

    con = connection_list.front();
    connection_list.pop_front();
    
    --m_FreeConnections;
    ++m_CurrentConnections;

    lock.unlock();

    return con;
}

// Release a connection back to the pool
bool Connection_Pool::ReleaseConnection(MYSQL* con)
{
    if( con == NULL )
    {
        return false;
    }

    lock.lock();

    connection_list.push_back(con);
    ++m_FreeConnections;
    --m_CurrentConnections;

    lock.unlock();

    // Post + 1
    reserve.post();
    return true;
}

// Destroying the connection pool
void Connection_Pool::DestroyPool()
{
    lock.lock();

    if( connection_list.size() > 0 )
    {
        list<MYSQL *>::iterator it;
        for( it = connection_list.begin(); it != connection_list.end(); ++it)
        {
            MYSQL *connection = *it;
            mysql_close(connection);
        }

        m_CurrentConnections = 0;
        m_FreeConnections = 0;
        connection_list.clear();
    }

    lock.unlock();

}

// Getting the number of free connections
int Connection_Pool::GetFreeConnections()
{
    return this->m_FreeConnections;
}

// *!* Using RAII to release/get the connection 

Connection_RAII::Connection_RAII(MYSQL **SQL, Connection_Pool *connection_pool)
{
    *SQL = connection_pool->GetConnection();
    
    con_RAII  = *SQL;
    pool_RAII = connection_pool;
}

Connection_RAII::~Connection_RAII()
{
    pool_RAII->ReleaseConnection(con_RAII);
}


