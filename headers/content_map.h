#ifndef HTTP_CONTENT_MAP_H
#define HTTP_CONTENT_MAP_H

/**
 * @struct ContentTypeMap
 * @brief A file extention and associated content type key-value pair
 */
typedef struct
{
    char key[8];    //!< The file extention
    char value[32]; //!< The associated content type
} ContentTypeMap;

/**
 * @brief Given a file extention, return the equivalent content type
 * @param ext The extention of the file
 * @return The content type for the given file extention
 */
const char *get_type_from_map(const char *ext);

#endif /* HTTP_CONTENT_MAP_H */
