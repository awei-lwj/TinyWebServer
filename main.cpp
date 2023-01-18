#include "config.h"

int main(int argc, char *argv[])
{
    string user = "root";
    string passwd = "9024";
    string database_name = "awei";

    // Parse command line arguments
    Config config;
    config.parse_args(argc, argv);

    WebServer server;

    // Initializing the server
    server.init(config.port, user, passwd, database_name, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    // LOG
    server.log_write();

    // Database
    server.sql_pool();

    // Thread pool
    server.thread_pool();

    // Trig mode
    server.trig_mode();

    // Event
    server.event_listen();

    // Run
    server.eventLoop();

    return 0;
}