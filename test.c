#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#define return_defer(x) do { return_value = (x); goto defer; } while (0)

#define MR_IMPLEMENTATION
#include "mr.h"

// Load a file into a malloc-ed buffer.
static char *load_file(const char *path, size_t *length)
{
    char *return_value = NULL;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return_defer(NULL);

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *str = malloc(file_size + 1);
    size_t bytes_read = fread(str, 1, file_size, f);
    if (bytes_read != file_size) return_defer(NULL);
    
    str[bytes_read] = '\0';
    
    if (length != NULL) *length = bytes_read;
    return_defer(str);
defer:
    if (f != NULL) fclose(f);
    if (return_value == NULL && str != NULL) free(str);
    return return_value;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <source-path>\n", argv[0]);
        return 1;
    }

    char *source_path = argv[1];
    size_t len = 0;
    char *str = load_file(source_path, &len);

    mr_file file = {
        .len = len,
        .cap = len,
        .str = str,
    };
    
    mr_replace_marker_contents(&file, "test", -1, "int foo(void) { return 42; }", -1);

    printf("%.*s\n", file.len, file.str);
    
    return 0;
}

// TODO: Implement Testing
// TODO: Add Tests
