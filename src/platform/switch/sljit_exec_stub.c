#include <stddef.h>

void* sljit_malloc_exec(size_t size) {
    (void)size;
    return NULL;
}

void sljit_free_exec(void* ptr) {
    (void)ptr;
}
