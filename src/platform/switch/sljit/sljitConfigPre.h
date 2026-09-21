#ifndef LOD_SWITCH_SLJIT_CONFIG_PRE_H
#define LOD_SWITCH_SLJIT_CONFIG_PRE_H

#include <stddef.h>

#define SLJIT_EXECUTABLE_ALLOCATOR 0
#define SLJIT_UTIL_STACK 0

#ifdef __cplusplus
extern "C" {
#endif

void* sljit_malloc_exec(size_t size);
void sljit_free_exec(void* ptr);

#ifdef __cplusplus
}
#endif

#define SLJIT_MALLOC_EXEC(size, exec_allocator_data) sljit_malloc_exec((size_t)(size))
#define SLJIT_FREE_EXEC(ptr, exec_allocator_data) sljit_free_exec((void*)(ptr))

#endif /* LOD_SWITCH_SLJIT_CONFIG_PRE_H */
