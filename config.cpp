#include "config.h"

Config::Config()
{
    port = 9096;

    LOGWrite = 0;   // the ways of writing in log

    TRIGMode = 0;    // listen fd LT + connection fd LT
    LISTENTrigmode  = 0; // listen fd LT
    CONNECTTrigmode = 0; // connect fd LT

    OPT_LINGER = 0;   

    sql_num    = 8;        // number of connections of sql
    thread_num = 8;

    close_log   = 0;       // 1 close / 0 open
    actor_model = 0;

}

void Config::parse_args(int argc, char *argv[])
{
    int opt;

    const char *str = "p:l:m:o:s:t:c:a:";

    while( (opt = getopt(argc,argv,str)) != -1 )
    {
        switch(opt)
        {
            case 'p':
            {
                port = atoi(optarg);
                break;
            }

            case 'l':
            {
                LOGWrite = atoi(optarg);
                break;
            }

            case 'm':
            {
                TRIGMode = atoi(optarg);
                break;
            }

            case 'o':
            {
                OPT_LINGER = atoi(optarg);
                break;
            }

            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }

            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }

            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }

            case 'a':
            {
                actor_model = atoi(optarg);
                break;
            }
            default:
            {
                break;
            }

        }

    }

}