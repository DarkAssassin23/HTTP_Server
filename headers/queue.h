#ifndef HTTP_QUEUE_H
#define HTTP_QUEUE_H

#include <stdint.h>

/**
 * @struct Connection
 * @brief Contain the components of a connection
 */
typedef struct
{
    int *socket;     //!< The connections socket
    uint32_t raw_ip; //!< The IP address of the connection
} Connection;

/**
 * @struct node
 * @brief Object to contain elements on the queue
 */
struct node
{
    struct node *next; //!< The next connection in the queue
    Connection *conn;  //!< The connection
};
typedef struct node node_t;

/**
 * @brief Add the socket for the new connection to the queue
 * @param client_socket The socket for the connection to add
 */
void enqueue(int *client_socket);

/**
 * @brief Add the connection to the queue
 * @param conn The connection to add
 */
void enqueue_conn(Connection *conn);

/**
 * @brief Pop the top element off the queue and return it
 * @return The pointer to a connection if there is one
 * @note Returns NULL if the queue is empty
 */
Connection *dequeue(void);

#endif /* HTTP_QUEUE_H */
