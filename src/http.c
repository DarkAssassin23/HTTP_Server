#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "content_map.h"
#include "defaults.h"
#include "http.h"
#include "stdio.h"
#include "utils.h"

#define ERR_SIZE 256
#define FOOT_SIZE 256
#define HEAD_SIZE 64
#define CONSOLE_WIDTH 80
#define INIT_DIR_ENTRIES 16

static const char HTTP_VER[] = "HTTP/1.1";
static const char ELLIPSES[] = " ... ";
static const char *REQ_STRS[] = { "N/A",     "GET",  "POST",  "HEAD",
                                  "OPTIONS", "PUT",  "PATCH", "DELETE",
                                  "CONNECT", "TRACE" };
static const int NUM_REQ_TYPES = sizeof(REQ_STRS) / sizeof(char *);

/// List of supported HTTP methods
static const uint8_t SUPPORTED[] = { REQUEST_TYPE_GET, REQUEST_TYPE_HEAD,
                                     REQUEST_TYPE_OPTIONS };
static const int NUM_SUPPORTED = sizeof(SUPPORTED) / sizeof(uint8_t);

/**
 * @enum FileStatusCodes
 * @brief Status codes for the return value of get_requested_file
 */
enum FileStatusCodes
{
    FILE_STATUS_CODES_SUCCESS = 0,
    FILE_STATUS_CODES_SERVER_ERR = 1,
    FILE_STATUS_CODES_FILE_ERR = 2
};

bool http_ending(const char *buff, size_t size)
{
    const char end_seq[] = "\r\n\r\n";
    if (size < sizeof(end_seq) + 1)
        return false;

    return strcmp(buff + (size - sizeof(end_seq) + 1), end_seq) == 0;
}

/**
 * @brief Print the log message to ensure it fits in the console
 * @param preamble The start of the log message
 * @param path The path to the file
 * @param ver Should the version be displayed
 */
static void log_message(const char *preamble, char *path, bool ver)
{
    char log[CONSOLE_WIDTH + 2] = { 0 };
    size_t pre_len = strlen(preamble);
    size_t path_len = strlen(path);
    snprintf(log, CONSOLE_WIDTH + 1, "%s%s", preamble, path);

    // Message is too long, truncate it
    if ((pre_len + path_len) > CONSOLE_WIDTH)
    {
        size_t start = CONSOLE_WIDTH - sizeof(ELLIPSES);
        start -= ver ? sizeof(HTTP_VER) : 0;
        sprintf(log + start, "%s%s\n", ELLIPSES, ver ? HTTP_VER : "");
    }
    size_t len = strlen(log);
    log[len] = (log[len - 1] != '\n') ? '\n' : '\0';
    printf("%s", log);
}

/**
 * @brief Log the users request to the console
 * @param req The HTTP request to log
 */
#ifndef VERBOSE // Silences compiler warning for this being unused
static void log_request(HttpRequest *req)
{
    size_t line_len = 0;
    char *line = NULL;
    FILE *fp = fmemopen(req->buff, strlen(req->buff), "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: Unable to print users request\n");
        return;
    }

    if (getline(&line, &line_len, fp) != -1)
    {
        char preamble[HEAD_SIZE] = { 0 };
        snprintf(preamble, HEAD_SIZE - 1, "Request from %s: ", req->ip);
        log_message(preamble, line, true);
    }

    if (line != NULL)
        free(line);
    fclose(fp);
}
#endif /* !VERBOSE */

void parse_reqest_type(HttpRequest *req)
{
    int type;
    char *dup = strdup(req->buff);
    if (dup == NULL)
    {
        perror("strdup");
        return;
    }

    // Compare the request against the list of possible HTTP requests
    char *request = strtok(dup, " ");
    for (type = 0; type < NUM_REQ_TYPES; type++)
    {
        if (strcmp(request, REQ_STRS[type]) == 0)
        {
            req->type = type;
            break;
        }
    }

#ifndef VERBOSE
    // If verbose is on, this gets logged, but if its not we still want
    // to log the request
    log_request(req);
#endif
    free(dup);
}

bool validate_http_ver(HttpRequest *req)
{
    char *dup = strdup(req->buff);
    if (dup == NULL)
    {
        perror("strdup");
        return false;
    }

    char *line = strtok(dup, "\r");
    size_t ver_start = (strlen(line) + 1) - sizeof(HTTP_VER);
    bool ret = strcmp(line + ver_start, HTTP_VER) == 0;

    free(dup);
    return ret;
}

void send_response(const char *buff, size_t size, int *sock)
{
    write(*sock, buff, size);
    close(*sock);
#ifdef VERBOSE
    printf("%s", buff);
    printf("closing connection...\n");
#endif
}

void send_error(const char *err, int *sock)
{
    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE);
    char time_str[HEAD_SIZE] = { 0 };
    char http_err[HEAD_SIZE] = { 0 };

    get_time(time_str);
    sprintf(http_err, "<h1>%s</h1>\n", err);
    sprintf(buffer,
            "HTTP/1.1 %s\nDate: %s\nServer: %s\n"
            "Content-Type: text/html; charset=UTF-8\nContent-Length: %zu\n\n",
            err, time_str, SERVER_NAME, strlen(http_err));
    strcat(buffer, http_err);

    send_response(buffer, strlen(buffer), sock);
}

/**
 * @brief Get the value for the Content-Type portion of the header
 * @param buffer The buffer to store the Content-Type result
 * @param file The file being accessed
 * @note Not checking for all types
 * @see get_type_from_map()
 */
static void get_content_type(char *buffer, const char *file)
{
    struct stat stats;
    stat(file, &stats);
    strcpy(buffer, "Content-Type: ");
    char *ext = strdup(get_filename_ext(file));
    if (ext == NULL)
    {
        strcat(buffer, "text/plain; charset=UTF-8\n");
        return;
    }

    ext = lowerstr(ext);
    if (strcmp(ext, "") == 0 && S_ISDIR(stats.st_mode))
        strcat(buffer, "text/html");
    else
        strcat(buffer, get_type_from_map(ext));
    strcat(buffer, "; charset=UTF-8\n");
    free(ext);
}

/**
 * @brief Generate the header to be sent back to the user
 * @param buffer The buffer to hold the header
 * @param fp File descriptor fo the file being sent
 * @param file The name of the file
 * @param code The response code from the server
 * @param type The type of request from the user
 */
static void generate_resp_head(char *buffer, FILE *fp, const char *file,
                               const char *code, uint8_t type)
{
    char time_str[HEAD_SIZE] = { 0 };
    char cont_type[HEAD_SIZE] = { 0 };
    char file_size[HEAD_SIZE] = { 0 };
    char allow_list[HEAD_SIZE] = { 0 };

    switch (type)
    {
        case REQUEST_TYPE_HEAD:
            get_content_type(cont_type, file);
            break;
        case REQUEST_TYPE_GET:
            sprintf(file_size, "Content-Length: %zu", get_file_size(fp));
            get_content_type(cont_type, file);
            break;
        case REQUEST_TYPE_OPTIONS:
            strcpy(allow_list, "Allow:");
            for (int x = 0; x < NUM_SUPPORTED; x++)
                sprintf(allow_list + strlen(allow_list), " %s,",
                        REQ_STRS[SUPPORTED[x]]);
            allow_list[strlen(allow_list) - 1] = '\n';
        default:
            break;
    }
    get_time(time_str);
    sprintf(buffer, "HTTP/1.1 %s\n%sDate: %s\nServer: %s\n%s%s\n\n", code,
            allow_list, time_str, SERVER_NAME, cont_type, file_size);
}

/**
 * @brief Get the contents of the directory
 * @param path The path to the directory to read.
 * @param items The number if items found in the directory
 * @return Array of dirent structs for each item in the directory
 * @attention If the function does not return NULL, the returned dirent struct
 * must be freed when done
 */
static struct dirent **get_dir_tree(const char *path, size_t *items)
{
    size_t max = INIT_DIR_ENTRIES;
    size_t i = 0;
    struct dirent **dir_elms = NULL, **tmp = NULL;
    DIR *d = opendir(path);
    if (d == NULL)
        goto get_dir_tree_end;

    struct dirent *dir;
    dir_elms = calloc(max, sizeof(struct dirent *));
    while ((dir = readdir(d)) != NULL)
    {
        // Make sure our list has enough space
        if (i == max)
        {
            size_t t_max = max * 2;
            tmp = realloc(dir_elms, (t_max * sizeof(struct dirent *)));
            if (tmp == NULL)
                goto get_dir_tree_close_d;
            dir_elms = tmp;
            max = t_max;
        }

        // Allocate space for the new entry
        dir_elms[i] = malloc(sizeof(struct dirent));
        if (dir_elms[i] == NULL)
            goto get_dir_tree_close_d;

        // Add the entry to the list
        memset(dir_elms[i], 0, sizeof(struct dirent));
        strcpy(dir_elms[i]->d_name, dir->d_name);
        dir_elms[i]->d_type = dir->d_type;
        i++;
    }
get_dir_tree_close_d:
    closedir(d);

get_dir_tree_end:
    *items = i;

    // Sort the contents to ensure directories are first
    // and everything is alphabetized
    qsort(dir_elms, i, sizeof(struct dirent *), compare_dir_elms);
    return dir_elms;
}

/**
 * @brief Create an HTML page displaying the contents of the directory
 * @param path The local path to the directory to display the contents of
 * @param full_path The full path to the directory
 * @param buffer Pointer to the buffer to store the HTML into
 * @param b_size Pointer to the current size of the buffer
 */
static void create_dir_html(const char *path, const char *full_path,
                            char **buffer, size_t *b_size)
{
    unsigned char slash = path[strlen(path) - 1] != '/';
    char file_path[PATH_MAX + 1] = { 0 };
    const char *align_right = "style=\"text-align: right\"";
    char end[] = "</table>\n</html>";

    // Header for the HTML page
    snprintf(*buffer, *b_size - 1,
             "<!DOCTYPE html>\n<head>"
             "<style>\ntd{\npadding-right: 30px;\ntext-align: left;\n}\n"
             "</style>\n</head>\n"
             "<h1>Index of %s%s</h1>\n"
             "<table>\n",
             path, slash ? "/" : "");
    size_t buff_len = strlen(*buffer);

    // Get all the files and directories in the given directory
    size_t items;
    struct dirent **dir = get_dir_tree(full_path, &items);
    if (dir == NULL)
        goto create_dir_html_end;

    // Create the HTML to display each entry in the given directory
    for (int x = 0; x < (int) items; x++)
    {
        if (strcmp(dir[x]->d_name, ".") == 0)
            continue;

        // Create the file path for the given entry
        size_t d_name_len = strlen(dir[x]->d_name) + 1;
        char *local_path = calloc(strlen(path) + d_name_len + slash,
                                  sizeof(char));
        if (local_path != NULL)
            sprintf(local_path, "%s%s%s", path, slash ? "/" : "",
                    dir[x]->d_name);
        snprintf(file_path, PATH_MAX, "%s/%s", full_path, dir[x]->d_name);

        // Allocate memory for all the HTML that will comprise this entry
        size_t row_max = (HEAD_SIZE * 3) + (d_name_len * 2)
                         + (strlen(align_right) * 2) + 1;
        char *table_row = calloc(row_max, sizeof(char));
        if (table_row == NULL)
        {
            free(local_path);
            goto create_dir_html_end;
        }

        // Allocate memory for the HTML that will comprise the file size
        char *size_str = calloc(strlen(align_right) + HEAD_SIZE, sizeof(char));
        if (size_str == NULL)
        {
            free(local_path);
            free(table_row);
            goto create_dir_html_end;
        }

        struct stat stats = { 0 };
        stat(file_path, &stats);

        // Get the time the file/directory was last modified
        char time_str[HEAD_SIZE] = { 0 };
#if __APPLE__
        strftime(time_str, sizeof(time_str), "%d-%b-%Y %R",
                 gmtime(&stats.st_mtimespec.tv_sec));
#else
        strftime(time_str, sizeof(time_str), "%d-%b-%Y %R",
                 gmtime(&stats.st_mtim.tv_sec));
#endif

        // Get the size of the file, if applicable
        sprintf(size_str, "<td %s>", align_right);
        if (stats.st_size && (dir[x]->d_type != DT_DIR))
            // Not casting throws a warning on macOS
            sprintf(size_str + strlen(size_str), "%ld</td>\n",
                    (long) stats.st_size);
        else
            sprintf(size_str + strlen(size_str), "-<td>\n");

        // Create an entry in the table for the current file/directory
        snprintf(table_row, row_max,
                 "<tr>\n"
                 "<td><a href=\"/%s%s\">%s%s</a></td>\n"
                 "<td>%s</td>\n"
                 "%s"
                 "</tr>\n",
                 local_path, (dir[x]->d_type == DT_DIR) ? "/" : "",
                 dir[x]->d_name, (dir[x]->d_type == DT_DIR) ? "/" : "",
                 time_str, size_str);

        // Make sure the buffer is large enough
        if (buff_resize(buffer, b_size, (buff_len + strlen(table_row) + 1))
            != 0)
        {
            free(local_path);
            free(table_row);
            free(size_str);
            free_dir_list(dir, items);
            return;
        }

        // Add the new entry's HTML to the total HTML
        strcat(*buffer, table_row);
        buff_len = strlen(*buffer);

        free(size_str);
        free(table_row);
        free(local_path);
    }

create_dir_html_end:
    // List of dirent structs is no longer needed
    free_dir_list(dir, items);

    // Make sure the buffer is large enough to hold the HTML footer
    if (buff_resize(buffer, b_size, (buff_len + sizeof(end) + 1)) != 0)
        return;

    // Add the HTML footer and remove any unneeded space
    strcat(*buffer, end);
    buff_shrink_to_fit(buffer, b_size);
}

/**
 * @brief Get the requested file from the HTTP request
 * @param buffer The buffer containing the HTTP request
 * @param ret The return code generated by the function.
 * @see FileStatusCodes
 * @return The file requested
 * @attention Return value must be freed
 */
char *get_requested_file(char *buffer, int *ret)
{
    size_t max_path = PATH_MAX - strlen(HTML_PATH);
    char file[PATH_MAX + 1] = { 0 };
    char *lineptr = NULL, *res = NULL, *word = NULL, *ver = NULL;
    size_t path_size = 0, line_len = 0;
    *ret = FILE_STATUS_CODES_SUCCESS;

    FILE *fp = fmemopen(buffer, strlen(buffer), "r");
    if (fp == NULL)
    {
        perror("fmemopen");
        *ret = FILE_STATUS_CODES_SERVER_ERR;
        goto get_requested_file_end;
    }

    if (getline(&lineptr, &line_len, fp) == -1)
    {
        perror("getline");
        *ret = FILE_STATUS_CODES_SERVER_ERR;
        goto get_requested_file_end;
    }

    word = strtok(lineptr, " ");
    while ((word = strtok(NULL, " ")) != NULL && path_size < max_path)
    {
        word = trim(word);
        // If we reached the HTTP version, we've parsed the whole filename
        if (strcmp(word, HTTP_VER) == 0)
            break;
        if (path_size != 0)
        {
            snprintf(file + path_size, PATH_MAX - path_size, "\\ %s", word);
            path_size += strlen(word) + 2; // One for backslash, one for space
        }
        else
        {
            // Remove forward slash
            if (word[0] == '/')
                word++;
            strncpy(file, word, PATH_MAX);
            path_size += strlen(word);
        }
    }

    if (path_size >= max_path)
    {
        *ret = FILE_STATUS_CODES_FILE_ERR;
        goto get_requested_file_end;
    }

    res = calloc(path_size + 1, sizeof(char));
    if (res == NULL)
    {
        perror("calloc");
        *ret = FILE_STATUS_CODES_SERVER_ERR;
    }
    else
        strcpy(res, file);

get_requested_file_end:
    if (fp != NULL)
        fclose(fp);
    free(lineptr);
    free(ver);
    return res;
}

void send_requested_file(HttpRequest *req, int *sock)
{
    bool malloced = false;
    char actual_path[PATH_MAX + 1] = { 0 };
    char full_path[PATH_MAX + 1] = { 0 };
    char *buff = NULL;
    char *def = "/.";
    char *dup = strdup(req->buff);
    if (dup == NULL)
    {
        perror("strdup");
        send_500_error(sock);
        goto send_requested_file_end;
    }

    // Parse the file requested from the request
    char *file = strtok(dup, " ");
    file = strtok(NULL, " ");
    if (strcmp(file, "/") == 0)
    {
        file = def;
        file++; // Remove the forward slash
    }
    else
    {
        int ret = 0;
        file = get_requested_file(req->buff, &ret);
        if (file == NULL)
        {
            switch (ret)
            {
                case FILE_STATUS_CODES_FILE_ERR:
                    send_431_error(sock);
                    break;
                case FILE_STATUS_CODES_SERVER_ERR:
                    send_500_error(sock);
                    break;
                default:
                    break;
            }
            goto send_requested_file_end;
        }
        malloced = true;
    }

    // Basic check to see if the request is invalid
    size_t len = strlen(file);
    if (len == 0) // Not checking this will result in memory read errors below
    {
        send_400_error(sock);
        goto send_requested_file_end;
    }
    if (file[len - 1] == '\r' || file[len - 1] == '\n')
    {
        send_400_error(sock);
        goto send_requested_file_end;
    }

    // Create the full path based on the configured HTML root directory
    snprintf(full_path, PATH_MAX, "%s/%s", HTML_PATH, file);

    // Validity check
    if (realpath(full_path, actual_path) == NULL)
    {
        log_message("ERROR(bad path): ", full_path, false);
        send_404_error(sock);
        goto send_requested_file_end;
    }

    // Make sure we have permission to read the file
    if (access(actual_path, R_OK) != 0)
    {
        log_message("ERROR(permission): ", actual_path, false);
        send_403_error(sock);
        goto send_requested_file_end;
    }

    // Verify we can open the file
    FILE *fp = fopen(actual_path, "r");
    if (fp == NULL)
    {
        log_message("ERROR(open): ", actual_path, false);
        send_500_error(sock);
        goto send_requested_file_end;
    }

    struct stat path_stat;
    stat(actual_path, &path_stat);

    // User requested a directory rather than a file
    if (S_ISDIR(path_stat.st_mode))
    {
        fclose(fp);

        // Create the path for index.html
        const char index[] = "/index.html";
        char *index_path = malloc(sizeof(index) + strlen(actual_path) + 1);
        if (index_path == NULL)
        {
            send_500_error(sock);
            goto send_requested_file_end;
        }
        sprintf(index_path, "%s%s", actual_path, index);

        // Check if index.html exists in this directory
        if (realpath(index_path, full_path) != NULL)
        {
            // Make sure we have permission to read the index.html file
            if (access(index_path, R_OK) == 0)
            {
                fp = fopen(index_path, "r");
                if (fp == NULL)
                {
                    send_500_error(sock);
                    free(index_path);
                    goto send_requested_file_end;
                }
                free(index_path);
                goto send_requested_file_send;
            }
            log_message("ERROR(permission): ", index_path, false);
            send_403_error(sock);
            free(index_path);
            goto send_requested_file_end;
        }
        free(index_path);

        // index.html does not exist in this directory,
        // show the directory's contents
        size_t size = BUFF_SIZE;
        char *buffer = calloc(size, sizeof(char));
        if (buffer == NULL)
        {
            send_500_error(sock);
            goto send_requested_file_end;
        }
        buff = buffer;
        create_dir_html(file, actual_path, &buff, &size);

        fp = fmemopen(buff, size, "r");
        if (fp == NULL)
        {
            send_500_error(sock);
            free(buffer);
            goto send_requested_file_end;
        }
    }

send_requested_file_send:
    // Send the requested file, or directory contents, back to the user
    send_200(sock, fp, actual_path, req->type);
    fclose(fp);
    if (buff != NULL)
        free(buff);
    close(*sock);

send_requested_file_end:
    if (malloced)
        free(file);
    free(dup);
}

/*=====================================*/
/*       Success Response Codes        */
/*=====================================*/

void send_200(int *sock, FILE *fp, const char *file, uint8_t type)
{
    size_t bytes_read = 0;
    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE);
    generate_resp_head(buffer, fp, file, "200 OK", type);
    write(*sock, buffer, strlen(buffer));

#ifdef VERBOSE
    printf("%s", buffer);
#endif

    // Read file contents and send them to the client
    memset(buffer, 0, BUFF_SIZE);
    while ((bytes_read = fread(buffer, 1, BUFF_SIZE - 1, fp)) > 0)
    {
        // Prevents an error if the client closed the socket before all the
        // data was sent
        send(*sock, buffer, bytes_read, MSG_NOSIGNAL);
    }

#ifdef VERBOSE
    printf("closing connection...\n");
#endif
}

void send_204(int *sock, uint8_t type)
{
    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE);
    generate_resp_head(buffer, NULL, NULL, "204 No Content", type);
    send_response(buffer, strlen(buffer), sock);
}

/*=====================================*/
/*        Error Response Codes         */
/*=====================================*/

void send_400_error(int *sock)
{
    send_error("400 Bad Request", sock);
}

void send_403_error(int *sock)
{
    send_error("403 Forbidden", sock);
}

void send_404_error(int *sock)
{
    send_error("404 File not found", sock);
}

void send_405_error(int *sock)
{
    send_error("405 Method Not Allowed", sock);
}

void send_408_error(int *sock)
{
    send_error("408 Request Timeout", sock);
}

void send_413_error(int *sock)
{
    send_error("413 Content Too Large", sock);
}

#ifdef TEAPOT
void send_418_error(int *sock)
{
    send_error("418 I'm a teapot", sock);
}
#endif /* TEAPOT */

void send_431_error(int *sock)
{
    send_error("431 Request Header Fields Too Large", sock);
}

/*=====================================*/
/*     Server Error Response Codes     */
/*=====================================*/
void send_500_error(int *sock)
{
    send_error("500 Internal Server Error", sock);
}

void send_505_error(int *sock)
{
    send_error("505 HTTP Version Not Supported", sock);
}
