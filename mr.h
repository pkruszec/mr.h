#ifndef MR_H
#define MR_H

#ifndef MRDEF
# define MRDEF static
#endif

// This string should appear at the beginning of the line of each marker.
#ifndef MR_MARK_PREFIX
# define MR_MARK_PREFIX "//!mr "
#endif

#ifndef MR_LOG_ERROR
# define MR_LOG_ERROR "ERROR: "
# define MR_LOG_INFO  "INFO: "
#endif

#ifndef MR_STR_LIT_LEN
# define MR_STR_LIT_LEN(s) (sizeof(s)/sizeof((s)[0]) - 1)
#endif

#ifndef MR_REALLOC
# include <stdlib.h>
# define MR_REALLOC(ptr, oldsz, newsz) realloc(ptr, newsz)
#endif

#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#ifndef MR_ASSERT
# include <assert.h>
# define MR_ASSERT(x) assert(x)
#endif

typedef struct {
    size_t len;
    size_t cap;
    char *str;
} mr_file;

typedef enum {
    MR_LE_NONE = 0,
    MR_LE_LF,
    MR_LE_CRLF,
} mr_line_ending;

typedef enum {
    MR_CMD_NONE = 0,
    MR_CMD_BEGIN,
    MR_CMD_END,
} mr_cmd_type;

extern void mr_logf(const char *fmt, ...);

// file->str should be able to be reallocated using MR_REALLOC().
// If marker_len < 0, expects `marker` to be null-terminated.
// If len < 0, expects `str` to be null-terminated.
// Returns 0 on failure.
MRDEF int mr_replace_marker_contents(mr_file *file, const char *marker, int marker_len, const char *str, int len);

#endif // MR_H

#ifdef MR_IMPLEMENTATION

#ifndef MR_OWN_LOGF
MRDEF void mr_logf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#endif

MRDEF int mr_is_whitespace(char c)
{
    return c == ' '
        || c == '\t'
        || c == '\n'
        || c == '\r';
}

MRDEF int mr_replace_marker_contents(mr_file *file, const char *marker, int marker_len, const char *str, int len)
{
    if (marker_len < 0) {
        marker_len = strlen(marker);
    }
    if (len < 0) {
        len = strlen(str);
    }
    
    mr_line_ending line_ending = MR_LE_NONE;
    size_t line = 0;
    size_t col  = 0;
    int is_this_line_cmd = 1;
    mr_cmd_type cmd_type = MR_CMD_NONE;

    char current_marker[256];
    size_t current_marker_len = 0;
    
    int our_marker_start_byte = -1;
    int our_marker_end_byte = -1;

    int start_on_next_line = 0;
    
    for (size_t i = 0; i < file->len; ++i) {
        // Handle line endings.
        if (line_ending == MR_LE_NONE) {
            // Establish what the line ending is.
            // Based on the first sequence of characters that looks like a line ending.
            if (file->str[i] == '\n') {
                if (i > 0 && file->str[i - 1] == '\r') {
                    line_ending = MR_LE_CRLF;
                } else {
                    line_ending = MR_LE_LF;
                }

                const char *line_ending_str = (line_ending == MR_LE_CRLF) ? "CRLF" : "LF";
                mr_logf(MR_LOG_INFO "Established line ending: %s\n", line_ending_str);
            }
        }

        int end_of_line = 0;
        if (line_ending == MR_LE_CRLF) {
            if (file->str[i] == '\n') {
                if (i > 0 && file->str[i - 1] == '\r') {
                    end_of_line = 1;
                }
            }
        } else if (line_ending == MR_LE_LF) {
            if (file->str[i] == '\n') {
                end_of_line = 1;
            }
        }

        if (end_of_line) {
            line += 1;
            col = 0;
            is_this_line_cmd = 1;
            continue;
        }

        if (start_on_next_line) {
            our_marker_start_byte = (int)i;
            cmd_type = MR_CMD_NONE;
            start_on_next_line = 0;
        }
        
        if (col < MR_STR_LIT_LEN(MR_MARK_PREFIX)) {
            if (file->str[i] != MR_MARK_PREFIX[col]) {
                is_this_line_cmd = 0;
                cmd_type = MR_CMD_NONE;
            }
        } else if (is_this_line_cmd) {
            char *tmp = &file->str[i];
            size_t tmp_len = file->len - i - 1;

            MR_ASSERT(tmp_len > 0);
            
            switch (cmd_type) {
                case MR_CMD_NONE: {
                    if (*tmp == '{') {
                        mr_logf(MR_LOG_INFO "Got begin (%d)\n", line+1);
                        cmd_type = MR_CMD_BEGIN;
                    } else if (*tmp == '}') {
                        mr_logf(MR_LOG_INFO "Got end (%d)\n", line+1);
                        cmd_type = MR_CMD_END;
                    } else {
                        mr_logf(MR_LOG_ERROR "(%d:%d): Expected '{' or '}', got '%c' (%d)\n", line+1, col+1, *tmp, *tmp);
                        return 0;
                    }
                } break;

                case MR_CMD_BEGIN: {
                    if (our_marker_start_byte >= 0) {
                        mr_logf(MR_LOG_ERROR "(%d:%d): Nested markers are not allowed.\n", line+1, col+1);
                        return 0;
                    }
                    
                    if (mr_is_whitespace(*tmp)) {
                        if (current_marker_len < 1) {
                            // Unrelevant whitespace, move on
                        } else {
                            mr_logf(MR_LOG_INFO "Got marker begin %.*s\n", current_marker_len, current_marker);

                            if (current_marker_len == marker_len && strncmp(current_marker, marker, marker_len) == 0) {
                                start_on_next_line = 1;
                                // our_marker_start_byte = (int)i + (int)line_ending;
                                // mr_logf(MR_LOG_INFO "Found our marker start at byte %d\n", our_marker_start_byte);
                            }
                        }
                    } else {
                        if (current_marker_len >= sizeof(current_marker)) {
                            mr_logf(MR_LOG_ERROR "(%d:%d): Marker names cannot be longer than %d bytes\n", line+1, col+1, sizeof(current_marker));
                            return 0;
                        }

                        current_marker[current_marker_len++] = *tmp;
                    }
                } break;

                case MR_CMD_END: {
                    if (current_marker_len > 0) {
                        if (our_marker_start_byte >= 0) {
                            our_marker_end_byte = (int)(i - col);

                            // If the marker contents are empty, we won't strip the newline.
                            // Not sure if we want to strip it anyway.
                            if (our_marker_end_byte - our_marker_start_byte > 0) {
                                our_marker_end_byte -= (int)line_ending;
                            }
                            goto after_loop;
                        }
                        
                        current_marker_len = 0;
                    } else {
                        mr_logf(MR_LOG_ERROR "(%d:%d): Unexpected end of marker.\n", line+1, col+1);
                        return 0;
                    }
                } break;
            }
        }
        
        col += 1;
    }

after_loop:
    if (our_marker_start_byte < 0) {
        mr_logf(MR_LOG_ERROR "Marker '%.*s' not found.\n", marker_len, marker);
        return 0;
    }

    if (our_marker_end_byte < 0) {
        mr_logf(MR_LOG_ERROR "Expected marker end, got end of line. Please add '%s}'.\n", MR_MARK_PREFIX);
        return 0;
    }

    mr_logf(MR_LOG_INFO "%d-%d\n", our_marker_start_byte, our_marker_end_byte);
    mr_logf(MR_LOG_INFO "Contents: '%.*s'\n", our_marker_end_byte - our_marker_start_byte, file->str + our_marker_start_byte);

    int contents_len = our_marker_end_byte - our_marker_start_byte;
    MR_ASSERT(contents_len >= 0);

    if (contents_len > 0) {
        // If the current contents are empty, we must add a new line to preserve two line comments and allow for future parsing.
        // This effectively disables appending a new line after the replaced contents when it is not needed.
        // We don't need the line ending value after here, so no problem just setting it.
        line_ending = 0;
    }
    
    int len_diff = len + (int)line_ending - contents_len;
    if (len_diff > 0) {
        size_t new_cap = file->cap + (size_t)len_diff;
        file->str = MR_REALLOC(file->str, file->cap, new_cap);

        if (file->str == NULL) {
            mr_logf(MR_LOG_ERROR "Could not reallocate memory with MR_REALLOC(). Check if file->str is allocated by the same allocator as MR_REALLOC().\n");
            return 0;
        }

        file->cap = new_cap;
    }

    char *src = file->str + our_marker_end_byte;
    char *dst = src + len_diff;
    size_t count = file->len - our_marker_end_byte;
    memmove(dst, src, count);
    file->len += len_diff;

    char *contents = file->str + our_marker_start_byte;
    memcpy(contents, str, len);

    if (line_ending == MR_LE_CRLF) {
        contents[len] = '\r';
        contents[len+1] = '\n';
    } else if (line_ending == MR_LE_LF) {
        contents[len] = '\n';
    }
    
    return 1;
}

#endif // MR_IMPLEMENTATION

