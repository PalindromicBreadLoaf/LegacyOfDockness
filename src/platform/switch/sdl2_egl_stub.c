#include <stddef.h>

/* SDL_egl.h pulls devkitPro's Mesa <EGL/egl.h>, whose <EGL/eglplatform.h> selects
   the *X11* branch on `defined(__unix__) || defined(USE_X11)` and then hard-fails
   on a missing <X11/Xlib.h>. ultramodern exports __unix__=1 PUBLIC for the
   moodycamel semaphore (S1.6), so any target linking it hands that define to this
   TU. EGL_NO_X11 is the header's own escape hatch and is simply true on Horizon. */
#define EGL_NO_X11 1

#include <SDL2/SDL_egl.h>

EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
    (void)api;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
                                       EGLConfig* configs, EGLint config_size,
                                       EGLint* num_config) {
    (void)dpy; (void)attrib_list; (void)configs; (void)config_size;
    if (num_config != NULL) {
        *num_config = 0;
    }
    return EGL_FALSE;
}

EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config,
                                        EGLContext share_context, const EGLint* attrib_list) {
    (void)dpy; (void)config; (void)share_context; (void)attrib_list;
    return EGL_NO_CONTEXT;
}

EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                               const EGLint* attrib_list) {
    (void)dpy; (void)config; (void)attrib_list;
    return EGL_NO_SURFACE;
}

EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                              EGLNativeWindowType win, const EGLint* attrib_list) {
    (void)dpy; (void)config; (void)win; (void)attrib_list;
    return EGL_NO_SURFACE;
}

EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    (void)dpy; (void)ctx;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    (void)dpy; (void)surface;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                                          EGLint attribute, EGLint* value) {
    (void)dpy; (void)config; (void)attribute;
    if (value != NULL) {
        *value = 0;
    }
    return EGL_FALSE;
}

EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) {
    (void)display_id;
    return EGL_NO_DISPLAY;
}

EGLint EGLAPIENTRY eglGetError(void) {
    return EGL_NOT_INITIALIZED;
}

EGLDisplay EGLAPIENTRY eglGetPlatformDisplay(EGLenum platform, void* native_display,
                                             const EGLAttrib* attrib_list) {
    (void)platform; (void)native_display; (void)attrib_list;
    return EGL_NO_DISPLAY;
}

__eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char* procname) {
    (void)procname;
    return NULL;
}

EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
    (void)dpy;
    if (major != NULL) {
        *major = 0;
    }
    if (minor != NULL) {
        *minor = 0;
    }
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                      EGLSurface read, EGLContext ctx) {
    (void)dpy; (void)draw; (void)read; (void)ctx;
    return EGL_FALSE;
}

EGLenum EGLAPIENTRY eglQueryAPI(void) {
    return EGL_NONE;
}

const char* EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) {
    (void)dpy; (void)name;
    return NULL;
}

EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    (void)dpy; (void)surface;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    (void)dpy; (void)interval;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) {
    (void)dpy;
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglWaitGL(void) {
    return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) {
    (void)engine;
    return EGL_FALSE;
}
