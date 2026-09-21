#pragma once

#define RTLD_NOW 0x0002
#define RTLD_LOCAL 0x0000

static inline void *dlopen(const char *file, int mode) { (void)file; (void)mode; return 0; }
static inline void *dlsym(void *handle, const char *name) { (void)handle; (void)name; return 0; }
static inline int dlclose(void *handle) { (void)handle; return 0; }
static inline const char *dlerror(void) { return "dlopen is not available"; }
