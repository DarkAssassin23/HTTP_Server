#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @struct ConfigOptions
 * @brief Configuration options for the server to use at run time
 */
typedef struct
{
    char server_name[24];    //!< Name for the server
    char path[PATH_MAX + 1]; //!< Path the HTML directory
    uint32_t timeout;        //!< Request timeout length (in milliseconds)
    uint16_t threads;        //!< Number of threads the server should run with
    uint16_t port;           //!< The port the server should run on
    uint16_t backlog;        //!< Max queue len for pending connections
    uint16_t buff_size;      //!< The size to use to create buffers
} ConfigOptions;

/**
 * @brief Write the current time to the provided string
 * @param time_str The string to store the time string in
 */
void get_time(char *time_str);

/**
 * @brief Get the size of the given file
 * @param fp File descriptor of the file to get the size of
 * @return File size (in bytes)
 */
size_t get_file_size(FILE *fp);

/**
 * @brief Get the files file extention
 * @param filename The name of the file to get the extention of
 * @return The file extention
 */
const char *get_filename_ext(const char *filename);

/**
 * @brief Return an initialized ConfigOptions struct
 * @return Initialized ConfigOptions
 */
ConfigOptions init_config_opts(void);

/**
 * @brief Parse the config file and return its properties
 * @param config File descriptor of the config file to read the contents of
 * @return Properties of how the server should be configured
 */
ConfigOptions parse_config(FILE *config);

/**
 * @brief Trim the leading and trailing white space from the string
 * @param str The string to trim
 * @return The string with no leading or trailing whitespace
 */
char *trim(char *str);

/**
 * @brief Return the given string in all lowercase
 * @param str The string to convert to lowercase
 * @return The given string in all lowercase
 */
char *lowerstr(char *str);

/**
 * @brief Resize the buffer if needed
 * @param buffer The buffer to resize
 * @param max The current maximum size of the buffer
 * @param required The minimum size the buffer needs to be
 * @return 0 on success, 1 if something went wrong
 */
int buff_resize(char **buffer, size_t *max, size_t required);

/**
 * @brief Shink the buffer to the minimum size required
 * @param buffer The buffer to shrink
 * @param size The resulting new size of the buffer
 * @return 0 on success, 1 if something went wrong
 */
int buff_shrink_to_fit(char **buffer, size_t *size);

/**
 * @brief Comparison function, for qsort, to sort elements in a directory
 *
 * Sorts directories followed by regular files, both in alphabetical order
 * @param a First directory element to compare
 * @param b Second directory element to compare
 * @return -1: (a < b), 0: (a == b), 1: (a > b)
 */
int compare_dir_elms(const void *a, const void *b);

/**
 * @brief Free the memory in the given list of directory contents
 * @param dir The list of dirent structs to free
 * @param size The number of dirent structs
 */
void free_dir_list(struct dirent **dir, size_t size);

/**
 * @brief Write the default http.conf file
 */
void gen_http_cfg(void);

#endif /* HTTP_UTILS_H */
