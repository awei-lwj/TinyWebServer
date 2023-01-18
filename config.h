#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

class Config
{
public:
    Config();
    ~Config() {};

    void parse_args(int argc, char *argv[]);

public:
    int port;         // port number

    int LOGWrite;     // log

    int TRIGMode;    // TRIG

    int LISTENTrigmode;   // listen fd mode

    int CONNECTTrigmode; // connect fd mode

    int OPT_LINGER;      // Closing the linger option

    int sql_num;       // the number of sql

    int thread_num;   // the number of threads

    int close_log;     // close the connection
    
    int actor_model;   // the actor model

};








#endif  /* CONFIG_H */