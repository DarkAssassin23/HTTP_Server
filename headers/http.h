#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @enum RequestType
 * @brief Enum for each of the diffrent HTTP request types
 * @ref https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
 */
enum RequestType
{
    REQUEST_TYPE_INVALID = 0,
    REQUEST_TYPE_GET = 1,
    REQUEST_TYPE_POST = 2,
    REQUEST_TYPE_HEAD = 3,
    REQUEST_TYPE_OPTIONS = 4,
    REQUEST_TYPE_PUT = 5,
    REQUEST_TYPE_PATCH = 6,
    REQUEST_TYPE_DELETE = 7,
    REQUEST_TYPE_CONNECT = 8,
    REQUEST_TYPE_TRACE = 9
};

/**
 * @struct HttpRequest
 * @brief Container to hold a single HTTP request
 */
typedef struct
{
    char ip[16];  //!< The IP address of the client
    char *buff;   //!< The full HTTP Request
    size_t size;  //!< The size of buff
    uint8_t type; //!< The RequestType for this HTTP request
} HttpRequest;

/**
 * @brief Check if ending of the buffer is the end of an HTTP request
 * @param buff The buffer to check
 * @param size The size of the buffer
 * @return True if the ending is that of an HTTP request
 */
bool http_ending(const char *buff, size_t size);

/**
 * @brief Parse the type of request this HTTP request and set the RequestType
 * @param req The HTTP Request to parse
 */
void parse_reqest_type(HttpRequest *req);

/**
 * @brief Validate the HTTP version
 * @param req The HTTP request
 * @return If the HTTP version is valid
 */
bool validate_http_ver(HttpRequest *req);

/**
 * @brief Send response to the client and close the socket
 * @param buff The buffer containing the response to send
 * @param size The size of the buffer
 * @param sock The socket to send to
 */
void send_response(const char *buff, size_t size, int *sock);

/**
 * @brief Send error message to the client and close the socket
 * @param err The error message to send
 * @param sock The socket to send to
 */
void send_error(const char *err, int *sock);

/**
 * @brief Send back the requested file from the GET request
 * @param req The HTTP request
 * @param sock The socket to send the request on
 */
void send_requested_file(HttpRequest *req, int *sock);

/*=====================================*/
/*       Success Response Codes        */
/*=====================================*/

/**
 * @brief Send 200 OK message to the client
 * @param sock The socket to send to
 * @param fp File descriptor fo the file being sent
 * @param file The name of the file
 * @param type The type of request from the user
 */
void send_200(int *sock, FILE *fp, const char *file, uint8_t type);

/**
 * @brief Send a 204 No Content message to the client and close the socket
 * @param sock The socket to send to
 * @param type The type of request from the user
 */
void send_204(int *sock, uint8_t type);

/*=====================================*/
/*        Error Response Codes         */
/*=====================================*/

/**
 * @brief Write an Bad Request message to the client and close the socket
 * @param sock The socket to send to
 */
void send_400_error(int *sock);

/**
 * @brief Send Forbidden message to the client and close the socket
 * @param sock The socket to send to
 */
void send_403_error(int *sock);

/**
 * @brief Send File Not Found message to the client and close the socket
 * @param sock The socket to send to
 */
void send_404_error(int *sock);

/**
 * @brief Send Method not allowed message to the client and close the socket
 * @param sock The socket to send to
 */
void send_405_error(int *sock);

/**
 * @brief Send Timeout message to the client and close the socket
 * @param sock The socket to send to
 */
void send_408_error(int *sock);

/**
 * @brief Request by the client is longer than the we are willing to interpret
 * @param sock The socket to send to
 */
void send_413_error(int *sock);

#ifdef TEAPOT
/**
 * @brief We refuses to brew coffee because we're, permanently, a teapot
 * @ref https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/418
 * @param sock The socket to send to
 */
void send_418_error(int *sock);
#endif /* TEAPOT */

/**
 * @brief Header field in the clients request is too long
 * @param sock The socket to send to
 */
void send_431_error(int *sock);

/*=====================================*/
/*     Server Error Response Codes     */
/*=====================================*/

/**
 * @brief Send a Server Error message to the client and close the socket
 * @param sock The socket to send to
 */
void send_500_error(int *sock);

/**
 * @brief Write an invalid HTTP Ver message to the client and close the socket
 * @param sock The socket to send to
 */
void send_505_error(int *sock);

#endif /* HTTP_H */
