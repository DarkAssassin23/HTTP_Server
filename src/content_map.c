#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content_map.h"

/**
 * @brief Mapping of file extentions and their content type
 * @note Not checking all types
 * @ref https://stackoverflow.com/a/48704300
 */
static const ContentTypeMap TYPE_MAP[] = {
    { "acc", "audio/mpeg" },      { "css", "text/css" },
    { "csv", "text/csv" },        { "f4v", "video/x-flv" },
    { "flv", "video/x-flv" },     { "gif", "image/gif" },
    { "html", "text/html" },      { "ico", "image/x-icon" },
    { "icon", "image/x-icon" },   { "java", "application/java-archive" },
    { "jpg", "image/jpeg" },      { "jpeg", "image/jpeg" },
    { "js", "text/javascript" },  { "json", "application/json" },
    { "m4v", "audio/mpeg" },      { "mov", "video/quicktime" },
    { "mp3", "audio/mpeg" },      { "mp4", "video/mp4" },
    { "mpeg", "video/mpeg" },     { "ogg", "application/ogg" },
    { "pdf", "application/pdf" }, { "png", "image/png" },
    { "tiff", "image/tiff" },     { "wav", "audio/wav" },
    { "webm", "video/webm" },     { "xml", "text/xml" },
    { "zip", "application/zip" }
};
static const int MAP_ELEMENTS = sizeof(TYPE_MAP) / sizeof(ContentTypeMap);

/**
 * @brief Compare function for bsearch
 * @param a The key we are comparing against (the extention)
 * @param b The element in the map we are comparing to
 * @return If the key is less, equal, or greater than the element
 */
static int compare_type(const void *a, const void *b)
{
    ContentTypeMap ext = *(ContentTypeMap *) a;
    ContentTypeMap element = *(ContentTypeMap *) b;
    return strcmp(ext.key, element.key);
}

const char *get_type_from_map(const char *ext)
{
    ContentTypeMap tmp = { 0 };
    strncpy(tmp.key, ext, sizeof(tmp.key) - 1);

    ContentTypeMap *cont = (ContentTypeMap *) bsearch(&tmp, TYPE_MAP,
                                                      MAP_ELEMENTS,
                                                      sizeof(ContentTypeMap),
                                                      compare_type);
    if (cont == NULL)
        return "text/plain";

    return cont->value;
}
