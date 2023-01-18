#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

// *!* This project is divided into two parts: Connection Pool and  Sign/Register function

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../locker/locker.h"
#include "../log/log.h"


using namespace std;
class Connection_Pool
{
public:
    MYSQL *GetConnection();   // Get the connection
    bool ReleaseConnection(MYSQL *connection); // Release the connection
    int GetFreeConnections();  // Get the free connection
    void DestroyPool(); // Destroy the connection pool

    // Single model
    static Connection_Pool *get_Instance(); // Get the instance

    // Initialize the connection pool
    void init(string url,string user,string password,
        string DataBaseName,int port,int maxConnections,int close_log); 

private:
    Connection_Pool();
    ~Connection_Pool();

    int m_MaxConnections;   // Max number of connections
    int m_CurrentConnections; // Current number of connections
    int m_FreeConnections; // Number of free connections

    locker lock;

    list<MYSQL *> connection_list; // List of connections
    sem reserve;

public:
    string m_url;   // URL of the connection pool
    string m_user;  // Username of the connection pool
    string m_port;  // Port of the connection pool
    string m_DataBaseName;  // Data base name
    string m_password; // Password of the connection pool

    int m_close_log;    // Close connection flag

};

class Connection_RAII
{
public:
    Connection_RAII(MYSQL **SQL, Connection_Pool *connection_pool);
    ~Connection_RAII();

private:
    MYSQL *con_RAII;
    Connection_Pool *pool_RAII;

};


#endif /* SQL_CONNECTION_POOL_H */
