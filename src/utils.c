#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defaults.h"
#include "limits.h"
#include "utils.h"

/**
 * @def MAX(a, b)
 * @brief Get the largest of two values
 * @param a First value to compare
 * @param b Second value to compare
 * @return The larger of the two values
 */
#define MAX(a, b) ((a > b) ? a : b)

void get_time(char *time_str)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char temp[64] = { 0 };
    // Example date output: Thu, 14 Nov 24, 15:04:45 PST
    strftime(temp, sizeof(temp), "%a, %d %b %y, %H:%M:%S %Z", tm);
    memcpy(time_str, temp, sizeof(temp));
}

size_t get_file_size(FILE *fp)
{
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return sz;
}

const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";

    return dot + 1;
}

ConfigOptions init_config_opts(void)
{
    ConfigOptions co = { 0 };
    strcpy(co.server_name, DEFAULT_SERVER_NAME);
    strcpy(co.path, DEFAULT_PATH);
    co.timeout = DEFAULT_TIMEOUT;
    co.threads = DEFAULT_THREAD_POOL_SIZE;
    co.port = DEFAULT_SERVER_PORT;
    co.backlog = DEFAULT_BACKLOG;
    co.buff_size = DEFAULT_BUFF_SIZE;
    return co;
}

ConfigOptions parse_config(FILE *config)
{
    ConfigOptions co = init_config_opts();
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    while ((nread = getline(&line, &len, config)) != -1)
    {
        // Ignore # (comments) and new lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        // Get the key and the value
        char *result = trim(line);
        char *value = strchr(result, ' ') + 1;
        char key[(value - result)];
        strncpy(key, lowerstr(result), sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        if (strcmp(key, "name") == 0)
            strncpy(co.server_name, value, sizeof(co.server_name) - 1);

        else if (strcmp(key, "html_root") == 0)
        {
            // Only set the root path if it is valid
            char tmp[PATH_MAX - 1] = { 0 };
            if (realpath(value, tmp) != NULL)
                strncpy(co.path, tmp, PATH_MAX);
        }
        else if (strcmp(key, "threads") == 0)
        {
            int threads = strtol(value, NULL, 10);
            if (threads <= 0)
                co.threads = DEFAULT_THREAD_POOL_SIZE;
            else
                co.threads = threads;
        }
        else if (strcmp(key, "port") == 0)
        {
            int port = strtol(value, NULL, 10);
            if (port <= 0)
                co.port = DEFAULT_SERVER_PORT;
            else
                co.port = port;
        }
        else if (strcmp(key, "timeout") == 0)
        {
            int timeout = strtol(value, NULL, 10);
            if (timeout <= 0)
                co.timeout = DEFAULT_TIMEOUT;
            else
                co.timeout = timeout;
        }
        else if (strcmp(key, "backlog") == 0)
        {
            int backlog = strtol(value, NULL, 10);
            if (backlog <= 0)
                co.backlog = DEFAULT_BACKLOG;
            else
                co.backlog = backlog;
        }
        else if (strcmp(key, "buff_size") == 0)
        {
            int buff_size = strtol(value, NULL, 10);
            if (buff_size <= 0)
                co.buff_size = DEFAULT_BUFF_SIZE;
            else
                co.buff_size = MAX(MIN_BUFF_SIZE, buff_size);
        }
    }
    free(line);
    return co;
}

char *trim(char *str)
{
    // Trim leading whitespace
    while (isspace((unsigned char) *str))
        str++;

    if (*str == 0)
        return str;

    // Trim trailing whitespace
    char *end = &str[strlen(str) - 1];
    do
    {
        end--;
    } while (isspace((unsigned char) *end));
    end++;

    // No end whitespace
    if (!isspace((unsigned char) *end))
        return str;

    // Set the first whitespace character after the string to null terminator
    *end = '\0';
    return str;
}

char *lowerstr(char *str)
{
    unsigned char *p = (unsigned char *) str;
    while (*p)
    {
        *p = tolower(*p);
        p++;
    }
    return str;
}

int buff_resize(char **buffer, size_t *max, size_t required)
{
    // Already have enough space
    if (required < (*max - 1))
        return 0;

    // Keep doubling until we have enough space
    size_t new_size = *max;
    do
    {
        new_size *= 2;
    } while ((new_size + 1) < required);

    // Try and resize
    char *tmp = realloc(*buffer, new_size);
    if (tmp == NULL)
        return 1;

    // Resize succeeded, update values
    *buffer = tmp;
    *max = new_size;
    return 0;
}

int buff_shrink_to_fit(char **buffer, size_t *size)
{
    // Check if we can shrink
    size_t len = strnlen(*buffer, *size) + 1;
    if ((len - 1) == *size)
        return 0;

    char *tmp = realloc(*buffer, len);
    if (tmp == NULL)
        return 1;

    // Ensure we null terminate (shouldn't be needed, but just to be safe)
    tmp[len - 1] = '\0';

    *buffer = tmp;
    *size = len;

    return 0;
}

int compare_dir_elms(const void *a, const void *b)
{
    struct dirent **e1 = (struct dirent **) a;
    struct dirent **e2 = (struct dirent **) b;

    // Both are either files or directories so they can be compared
    if ((*e1)->d_type == (*e2)->d_type)
    {
        int result = 0;
        char *n1 = NULL, *n2 = NULL;

        // Duplicate the names so we can convert them to lowercase without
        // modifying the original file/directory name
        n1 = strdup((*e1)->d_name);
        if (n1 == NULL)
        {
            // strdup failed, try and compare what we have
            return strcmp((*e1)->d_name, (*e2)->d_name);
        }

        n2 = strdup((*e2)->d_name);
        if (n2 == NULL)
        {
            // strdup failed, try and compare what we have
            free(n1);
            return strcmp((*e1)->d_name, (*e2)->d_name);
        }

        // Ensure the names are all lowercase and compare
        result = strcmp(lowerstr(n1), lowerstr(n2));
        free(n1);
        free(n2);
        return result;
    }
    // e1 is a directory, and e2 is not, so e1 comes first
    else if ((*e1)->d_type == DT_DIR && (*e2)->d_type != DT_DIR)
        return -1;
    else // e2 is a directory, and e1 is not, so e2 comes first
        return 1;
}

void free_dir_list(struct dirent **dir, size_t size)
{
    for (int x = 0; x < (int) size; x++)
        free(dir[x]);
    free(dir);
}

void gen_http_cfg(void)
{
    FILE *cfg = fopen(CFG_FILE, "w");
    if (cfg != NULL)
    {
        fprintf(cfg, "##### HTTP Server Config File #####\n\n");
        fprintf(cfg, "# The name you wish to call the server.\n# name %s\n\n",
                DEFAULT_SERVER_NAME);
        fprintf(cfg,
                "# The location where the HTTP servers files are located.\n"
                "# html_root %s\n\n",
                DEFAULT_PATH);
        fprintf(cfg,
                "# The number of threads you want the server to run with."
                "\n# threads %d\n\n",
                DEFAULT_THREAD_POOL_SIZE);
        fprintf(cfg,
                "# The port you want the server to run on.\n# port %d\n\n",
                DEFAULT_SERVER_PORT);
        fprintf(cfg,
                "# The amount of time (in milliseconds) before the "
                "connection times out.\n# timeout %d\n\n",
                DEFAULT_TIMEOUT);
        fprintf(cfg,
                "# The maximum length to which the queue of pending "
                "connections for sockfd\n# may grow.\n# backlog %d\n\n",
                DEFAULT_BACKLOG);
        fprintf(cfg,
                "# The size each buffer should be for reading and writing "
                "messages to the\n# client. If a value less than the minimum "
                "buffer size (%d) is entered, it\n# will force the buffer "
                "size to be %d.\n# buff_size %d\n\n",
                MIN_BUFF_SIZE, MIN_BUFF_SIZE, DEFAULT_BUFF_SIZE);
        fclose(cfg);
    }
}
