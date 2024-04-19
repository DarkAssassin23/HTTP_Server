#include <stdlib.h>

#include "queue.h"

node_t *head = NULL;
node_t *tail = NULL;

void enqueue(int *client_socket)
{
    node_t *new_node = malloc(sizeof(node_t));
    if (new_node == NULL)
        return;

    new_node->conn = malloc(sizeof(Connection));
    if (new_node->conn == NULL)
    {
        free(new_node);
        return;
    }

    new_node->conn->socket = client_socket;
    new_node->conn->raw_ip = 0;
    new_node->next = NULL;

    if (tail == NULL)
        head = new_node;
    else
        tail->next = new_node;

    tail = new_node;
}

void enqueue_conn(Connection *conn)
{
    node_t *new_node = malloc(sizeof(node_t));
    if (new_node == NULL)
        return;

    new_node->conn = conn;
    new_node->next = NULL;

    if (tail == NULL)
        head = new_node;
    else
        tail->next = new_node;

    tail = new_node;
}

Connection *dequeue(void)
{
    if (head == NULL)
        return NULL;

    Connection *result = head->conn;
    node_t *temp = head;
    head = head->next;

    if (head == NULL)
        tail = NULL;

    free(temp);
    return result;
}
