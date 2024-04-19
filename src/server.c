#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // Needed for alpine docker build
#include <time.h>
#include <unistd.h>

#include "defaults.h"
#include "http.h"
#include "queue.h"
#include "utils.h"

#define SOCKET_ERROR (-1)
#define SEC_TO_MS 1000
#define MICRO_TO_MS 1000
#define NANO_TO_MS (0.000001)

#ifdef TEAPOT
#define COUNT_RESET 0x7134 // Reset the teapot response count
uint16_t count = 0;        // Number of connections, used for teapot
const int TEAPOT_COND1 = (COUNT_RESET >> 6) - (2 << 4); // Figure it out
const int TEAPOT_COND2 = (COUNT_RESET >> 8) ^ 0x34;     // Figure it out
#endif

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

bool running = true;
char *SERVER_NAME = NULL;
char *HTML_PATH = NULL;
uint16_t SERVER_PORT = DEFAULT_SERVER_PORT;
uint16_t THREAD_POOL_SIZE = DEFAULT_THREAD_POOL_SIZE;
uint16_t SERVER_BACKLOG = DEFAULT_BACKLOG;
uint16_t BUFF_SIZE = DEFAULT_BUFF_SIZE;
uint32_t CONN_TIMEOUT_LEN = DEFAULT_TIMEOUT;

pthread_t *thread_pool = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

/**
 * @brief Initialize the server by parsing and setting the config options
 */
void init_server(void);

/**
 * @brief Print the running config of the server
 */
void print_running(void);

/**
 * @brief Handler for the SIGINT (CTRL + C) signal
 *
 * Used to gracefully exit by freeing all memory and joining all threads
 * @param signal The incoming signal
 */
void SIGINT_handler(int signal);

/**
 * @brief Join the all threads in the thread pool
 */
void join_thread_pool(void);

/**
 * @brief Basic error checker
 * @param exp Socket return to check for errors
 * @param msg Message to print in the event of an error
 * @return exp
 */
int check(int exp, const char *msg);

/**
 * @brief Function to manage the thread pool and check if there is work to do
 * @param arg Args passed in to be used by the thread (unused)
 * @return NULL
 */
void *thread_function(void *arg);

/**
 * @brief Function to handle what to do with an incoming connection
 * @param pclient The connection
 * @return NULL
 */
void *handle_connection(void *pclient);

/**
 * @brief Free memory allocated to global strings
 */
void free_strings(void);

int main(int argc, char **argv)
{
    int server_sock, client_sock, addr_size;
    SA_IN server_addr, client_addr;

    init_server();

    // Capture SIGINT (CTRL + C) so we can exit gracefully
    signal(SIGINT, SIGINT_handler);

    // Create a TCP socket and check if it failed or not
    check((server_sock = socket(AF_INET, SOCK_STREAM, 0)),
          "Failed to create socket");

    // Initialize address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    int optval = 1;

    // Add options to our socket
    check(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
                     sizeof(optval)),
          "Setting socket options failed\n");

    // Binds the socket to the port
    check(bind(server_sock, (SA *) &server_addr, sizeof(server_addr)),
          "Bind Failed");

    // Listens on that port
    check(listen(server_sock, SERVER_BACKLOG), "Listen Failed");

#ifndef VERBOSE
    // Used to let you know the server is running and not stalled
    printf("Waiting for connections...\n");
#endif

    while (running)
    {
#ifdef VERBOSE
        printf("Waiting for connections...\n");
#endif
        // Wait for and accept incoming connections
        addr_size = sizeof(SA_IN);
        check((client_sock = accept(server_sock, (SA *) &client_addr,
                                    (socklen_t *) &addr_size)),
              "Accept Failed");

        // Sets a timeout for the socket
        struct timeval tv;
        tv.tv_sec = CONN_TIMEOUT_LEN / SEC_TO_MS;
        tv.tv_usec = (CONN_TIMEOUT_LEN % SEC_TO_MS) * MICRO_TO_MS;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv,
                   sizeof(tv));

#ifdef VERBOSE
        // Prints out IP Address of the connected client
        printf("Connected to %s\n", inet_ntoa(client_addr.sin_addr));
#endif

        // Allocate memory for the new connection
        Connection *pclient = malloc(sizeof(Connection));
        if (pclient == NULL)
        {
            perror("malloc");
            break;
        }
        pclient->socket = malloc(sizeof(int));
        if (pclient->socket == NULL)
        {
            free(pclient);
            perror("malloc");
            break;
        }

        // Puts the connection in queue for thread to pull from
        *pclient->socket = client_sock;
        pclient->raw_ip = client_addr.sin_addr.s_addr;
        pthread_mutex_lock(&mutex);
        enqueue_conn(pclient);
        pthread_cond_signal(&cond_var);
        pthread_mutex_unlock(&mutex);
    }

    return 0;
}

void init_server(void)
{
    // Initialize the default config options
    ConfigOptions co = init_config_opts();
    SERVER_NAME = malloc(sizeof(co.server_name));
    if (SERVER_NAME == NULL)
    {
        perror("malloc");
        exit(1);
    }
    HTML_PATH = malloc(sizeof(co.path));
    if (HTML_PATH == NULL)
    {
        perror("malloc");
        free(SERVER_NAME);
        exit(1);
    }
    strcpy(SERVER_NAME, DEFAULT_SERVER_NAME);
    strcpy(HTML_PATH, DEFAULT_PATH);

    // Load the config options from the config file (if applicable)
    FILE *cfg = fopen(CFG_FILE, "r");
    if (cfg != NULL)
    {
        co = parse_config(cfg);
        SERVER_PORT = co.port;
        THREAD_POOL_SIZE = co.threads;
        CONN_TIMEOUT_LEN = co.timeout;
        SERVER_BACKLOG = co.backlog;
        BUFF_SIZE = co.buff_size;
        strcpy(SERVER_NAME, co.server_name);
        strcpy(HTML_PATH, co.path);
        fclose(cfg);
    }
    else // No config exists, make one
        gen_http_cfg();

    // Create thread pool
    thread_pool = malloc(sizeof(pthread_t) * THREAD_POOL_SIZE);
    if (thread_pool == NULL)
    {
        perror("malloc");
        free_strings();
        exit(1);
    }
    for (int x = 0; x < THREAD_POOL_SIZE; x++)
        pthread_create(&thread_pool[x], NULL, thread_function, NULL);

#ifdef VERBOSE
    print_running();
#endif
}

void print_running(void)
{
    printf("Running Config:\n");
    printf(" - Server Name:               %s\n", SERVER_NAME);
    printf(" - HTML Root:                 %s\n", HTML_PATH);
    printf(" - Server Port:               %d\n", SERVER_PORT);
    printf(" - Number of Threads:         %d\n", THREAD_POOL_SIZE);
    printf(" - Connection Timeout Length: %dms\n", CONN_TIMEOUT_LEN);
    printf(" - Backlog length:            %d\n", SERVER_BACKLOG);
    printf(" - Buffer size:               %d\n", BUFF_SIZE);
}

void SIGINT_handler(int signal)
{
    running = false;
#ifdef VERBOSE
    printf("\nCaught signal: %d\nShutting down...\n", signal);
#endif
    join_thread_pool();
    free_strings();
    exit(EXIT_SUCCESS);
}

void join_thread_pool(void)
{
    if (running)
        return;

    // Add data to the queue so the threads will join
    for (int x = 0; x < THREAD_POOL_SIZE; x++)
    {
        int *dummy = malloc(sizeof(int));
        if (dummy == NULL)
        {
            perror("malloc");
            exit(1);
        }

        *dummy = SOCKET_ERROR;
        pthread_mutex_lock(&mutex);
        enqueue(dummy);
        pthread_cond_signal(&cond_var);
        pthread_mutex_unlock(&mutex);
    }

    for (int x = 0; x < THREAD_POOL_SIZE; x++)
        pthread_join(thread_pool[x], NULL);

    // Free thread pool memory and any items remaining in the queue
    free(thread_pool);
    Connection *p;
    while ((p = dequeue()) != NULL)
    {
        free(p->socket);
        free(p);
    }
}

int check(int exp, const char *msg)
{
    if (exp == SOCKET_ERROR)
    {
        running = false;
        perror(msg);
        join_thread_pool();
        free_strings();
        exit(-1);
    }
    return exp;
}

void *thread_function(void *arg)
{
    while (running)
    {
        Connection *pclient;
        pthread_mutex_lock(&mutex);
        if ((pclient = dequeue()) == NULL)
        {
            pthread_cond_wait(&cond_var, &mutex);
            pclient = dequeue();
        }
        pthread_mutex_unlock(&mutex);
        if (pclient != NULL)
        {
            // We have a connection
            handle_connection(pclient);
        }
    }
    return NULL;
}

void *handle_connection(void *pclient)
{
    Connection *tmp = (Connection *) pclient;
    Connection conn = *tmp;
    int client_sock = *conn.socket;

    // Free pointers since we don't need them
    free(tmp->socket);
    free(tmp);

    if (client_sock == SOCKET_ERROR)
        return NULL;

#ifdef TEAPOT
    count++;
    if (count % COUNT_RESET == 0 || count % TEAPOT_COND1 == 0
        || count % TEAPOT_COND2 == 0)
    {
        send_418_error(&client_sock);
        count = (count == COUNT_RESET) ? 0 : count;
        return NULL;
    }
#endif /* TEAPOT */

    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE);
    size_t bytes_read = 0;
    size_t msg_size = 0;

    // Create timers to check for connection timeout
    struct timespec t_start = { 0, 0 }, t_end = { 0, 0 };
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    // Read the client's message up to BUFF_SIZE, until the end of a HTTP
    // request, or an error occurs (this includes the connection timing out)
    while ((bytes_read = read(client_sock, buffer, BUFF_SIZE)) > 0)
    {
        msg_size += bytes_read;

        // Calculate the time passed in milliseconds to determine if this is
        // a timeout
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        time_t start = (t_start.tv_sec * SEC_TO_MS)
                       + (t_start.tv_nsec * NANO_TO_MS);
        time_t end = (t_end.tv_sec * SEC_TO_MS) + (t_end.tv_nsec * NANO_TO_MS);
        time_t millis = end - start;
        if (millis >= CONN_TIMEOUT_LEN)
        {
            // Request timeout
            send_408_error(&client_sock);
            return NULL;
        }
        else if (msg_size > (BUFF_SIZE - 1))
        {
            // Message to large
            send_413_error(&client_sock);
            return NULL;
        }
        else if (http_ending(buffer, msg_size))
        {
            // Possibly valid message, go handle it
            break;
        }
        else
        {
            // Bad message format
            send_400_error(&client_sock);
            return NULL;
        }
    }

    check(bytes_read, "Recieve Error");
    if (msg_size < 1) // If this is zero the below will crash
        return NULL;

    buffer[msg_size - 1] = 0; // Ensure message is null terminated

#ifdef VERBOSE
    printf("%s\n", buffer);
#endif

    // Create the HTTP request
    struct in_addr addr = { conn.raw_ip };
    HttpRequest req = { 0 };
    strncpy(req.ip, inet_ntoa(addr), sizeof(req.ip) - 1);
    req.buff = buffer;
    req.size = msg_size;
    req.type = REQUEST_TYPE_INVALID;

    parse_reqest_type(&req);
    if (!validate_http_ver(&req))
    {
        send_505_error(&client_sock);
        return NULL;
    }
    switch (req.type)
    {
        case REQUEST_TYPE_GET:
        case REQUEST_TYPE_HEAD:
            send_requested_file(&req, &client_sock);
            break;
        case REQUEST_TYPE_OPTIONS:
            send_204(&client_sock, req.type);
            break;
        default:
            send_405_error(&client_sock);
            break;
    }

    fflush(stdout);
    return NULL;
}

void free_strings(void)
{
    free(SERVER_NAME);
    free(HTML_PATH);
}
