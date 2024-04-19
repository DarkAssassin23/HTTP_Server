#ifndef HTTP_CONF_DEFAULTS_H
#define HTTP_CONF_DEFAULTS_H

#include <stdint.h>

#ifdef DOCKER
#define CFG_FILE "/etc/http_server/http.conf"
#else
#define CFG_FILE "http.conf"
#endif /* DOCKER */
#define DEFAULT_SERVER_NAME "HTTP Server"
#define DEFAULT_PATH "/var/www/html"
#define DEFAULT_SERVER_PORT 4080
#define DEFAULT_THREAD_POOL_SIZE 20
#define DEFAULT_TIMEOUT 1000 // 1000 milliseconds
#define DEFAULT_BACKLOG 100
#define DEFAULT_BUFF_SIZE 4096 // In bytes
#define MIN_BUFF_SIZE 2048     // In bytes

extern char *SERVER_NAME;         //!< The name of the server
extern char *HTML_PATH;           //!< Path to the root HTML directory
extern uint16_t SERVER_PORT;      //!< Port the webserver will be available on
extern uint16_t THREAD_POOL_SIZE; //!< Number of threads in the thread pool
extern uint16_t SERVER_BACKLOG;   //!< Max queue len for pending connections
extern uint16_t BUFF_SIZE;        //!< Buffer size for in/out messages
extern uint32_t CONN_TIMEOUT_LEN; //!< Timeout for socket (unit: ms)

#endif /* HTTP_CONF_DEFAULTS_H */
