#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <GL/glx.h>

#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <linux/joystick.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <limits.h>
#include <signal.h>
#include <execinfo.h>
#include <string.h>

#include "handmade.h"

#include "handmade_keyboard.c"
#include "linux_keyboard.c"

static Atom WM_DELETE_WINDOW;

typedef struct LinuxContext {
    Display* dpy;
    Window window;
    XIC inputContext;
    XImage x11_backbuffer;

    bool32 shouldExit;
} LinuxContext;

internal uint64 getFileSize_(const char* filepath, bool32* success) {
    struct stat file_stat;
    if (stat(filepath, &file_stat) < 0) {
        if (success) {
            *success = false;
        }
        return 0;
    }

    if (success) {
        *success = true;
    }
    return file_stat.st_size;
}

internal bool32 readFileIntoMemory_(const char* filepath, void* buffer, uint64 size) {
    int fd = open(filepath, O_RDONLY);

    if (fd < 0) {
        return false;
    }

    if (read(fd, buffer, size) < 0) {
        return false;
    }

    close(fd);

    return true;
}

internal void outputBufferInConsole_(uint8* buffer, uint64 size) {
    write(STDOUT_FILENO, buffer, size);
}

internal OffscreenBuffer copy_x11_backbuffer(XImage x11_backbuffer) {
    OffscreenBuffer backbuffer;

    backbuffer.width = x11_backbuffer.width;
    backbuffer.height = x11_backbuffer.height;
    backbuffer.data = (uint32*)x11_backbuffer.data;

    return backbuffer;
}

internal void processInputEvents(LinuxContext* ctx, OffscreenBuffer* backbuffer, InputInfo* inputInfo) {
    inputInfo->didResize = false;
    
    // read X11 events
    while (XPending(ctx->dpy)) {
        XEvent x_evt;
        XNextEvent(ctx->dpy, &x_evt);

        // Filter out stuff like Compose keys in order for X to
        // give us the correct input further down the line
        if(XFilterEvent(&x_evt, None)) continue;

        InputEvent* evt = &inputInfo->events[inputInfo->eventCount];

        // NOTE(octave) : we could check that x_evt.window is our
        // window, but that is not necessary for now since we only
        // have one window
        switch (x_evt.type) {
        case DestroyNotify:
            ctx->shouldExit = true;
            break;
        case ClientMessage:
            if ((Atom)x_evt.xclient.data.l[0] == WM_DELETE_WINDOW) {
                XDestroyWindow(ctx->dpy, ctx->window);
                ctx->shouldExit = true;
            }
            break;
        case ConfigureNotify:
            backbuffer->width = x_evt.xconfigure.width;
            backbuffer->height = x_evt.xconfigure.height;
            
            inputInfo->didResize = true;
            inputInfo->state.windowWidth = backbuffer->width;
            inputInfo->state.windowHeight = backbuffer->height;
            break;

        case KeyPress: 
        case KeyRelease: {
            bool32 haveRepeat = false;
            // NOTE(octave) : if this is slow, might want to pass
            // QueuedAlready or QueuedAfterReading
            if (x_evt.type == KeyRelease
                && XEventsQueued(ctx->dpy, QueuedAfterFlush)) {
                XEvent x_nextEvent;
                XPeekEvent(ctx->dpy, &x_nextEvent);

                if (x_nextEvent.type == KeyPress
                    && x_nextEvent.xkey.time == x_evt.xkey.time
                    && x_nextEvent.xkey.keycode == x_evt.xkey.keycode) {
                    // it's a key repeat, pop the next event with it
                    XNextEvent(ctx->dpy, &x_nextEvent);
                    haveRepeat = true;
                }
            }
            
            Status status = 0;
            KeySym keysym;
            char buffer[4] = {};
            Xutf8LookupString(ctx->inputContext,
                              &x_evt.xkey,
                              buffer,
                              4,
                              &keysym,
                              &status);
            if (status == XBufferOverflow) {
                fprintf(stderr, "Buffer overflow while reading keyboard input.\n");
                break;
            }

            // bool32 have_keysym = (status == XLookupKeySym || status == XLookupBoth);
            bool32 have_buffer = (status == XLookupChars || status == XLookupBoth);

            evt->type = EVENT_KEY;
            evt->key.pressFlag = haveRepeat ? REPEAT
                : (x_evt.type == KeyPress) ? PRESS
                : RELEASE;
            if (x_evt.xkey.keycode < ARRAY_COUNT(x11KeyCodeToKeyIndex)) {
                evt->key.index = x11KeyCodeToKeyIndex[x_evt.xkey.keycode];
            } else {
                evt->key.index = KID_UNKNOWN;
            }

            // TODO(octave) : if it's a key repeat, copy the previous utf8 buffer
            if (have_buffer) {
                evt->key.utf8[0] = buffer[0];
                evt->key.utf8[1] = buffer[1];
                evt->key.utf8[2] = buffer[2];
                evt->key.utf8[3] = buffer[3];
            } else {
                evt->key.utf8[0] = 0;
                evt->key.utf8[1] = 0;
                evt->key.utf8[2] = 0;
                evt->key.utf8[3] = 0;
            }

            inputInfo->eventCount++;
            break;
        }

        case MotionNotify: {
            evt->type = EVENT_MOUSEMOVE;
            evt->mouseMove.x = x_evt.xmotion.x;
            evt->mouseMove.y = x_evt.xmotion.y;
            evt->mouseMove.xPrev = inputInfo->state.mouse.x;
            evt->mouseMove.yPrev = inputInfo->state.mouse.y;

            inputInfo->state.mouse.x = x_evt.xmotion.x;
            inputInfo->state.mouse.y = x_evt.xmotion.y;
            
            inputInfo->eventCount++;
            break;
        }
        case ButtonPress:
        case ButtonRelease: {
            if (x_evt.xbutton.button <= MOUSEBUTTON_COUNT) {
                uint8 mask = (1 << (x_evt.xbutton.button - 1));
                evt->type = EVENT_MOUSEBUTTON;
                evt->mouseButton.button = (MouseButton)mask;
                evt->mouseButton.x = x_evt.xbutton.x;
                evt->mouseButton.y = x_evt.xbutton.y;
                
                if (x_evt.xbutton.type == ButtonPress) {
                    inputInfo->state.mouse.buttonsDown |= mask;
                    evt->mouseButton.flag = PRESS;
                } else {
                    inputInfo->state.mouse.buttonsDown &= ~mask;
                    evt->mouseButton.flag = RELEASE;
                }

                inputInfo->eventCount++;
            } else if (x_evt.xbutton.button == Button4) {
                if (x_evt.xbutton.type == ButtonPress) {
                    evt->type = EVENT_MOUSEWHEEL;
                    evt->mouseWheel.scrollAmount = -1;

                    inputInfo->eventCount++;
                }
            } else if (x_evt.xbutton.button == Button5) {
                if (x_evt.xbutton.type == ButtonPress) {
                    evt->type = EVENT_MOUSEWHEEL;
                    evt->mouseWheel.scrollAmount = 1;

                    inputInfo->eventCount++;
                }
            } 
            break;
        }
        }
    }

    // handle size changes only once per frame
    if (inputInfo->didResize) {
        ctx->x11_backbuffer.width = backbuffer->width;
        ctx->x11_backbuffer.height = backbuffer->height;
        ctx->x11_backbuffer.bytes_per_line = backbuffer->width * 4;
        ctx->x11_backbuffer.data = (char*)realloc(ctx->x11_backbuffer.data,
                                                  ctx->x11_backbuffer.bytes_per_line * ctx->x11_backbuffer.height);
        *backbuffer = copy_x11_backbuffer(ctx->x11_backbuffer);
    }

    // read keyboard state
    char xKeysDown[32];
    XQueryKeymap(ctx->dpy, xKeysDown);

    for (uint32 i = 0; i < 32 * 8; i++) {
        uint32 x11ByteIndex = i / 8;
        char x11BitMask = 1 << (i % 8);

        KeyIndex keyIndex = x11KeyCodeToKeyIndex[i];
        uint32 ourByteIndex = keyIndex / 64;
        uint64 ourBitMask = 1ul << (keyIndex % 64);

        if (keyIndex != KID_UNKNOWN) {
            if (xKeysDown[x11ByteIndex] & x11BitMask) {
                inputInfo->state.keyboard.keysDown[ourByteIndex] |= ourBitMask;
            } else {
                inputInfo->state.keyboard.keysDown[ourByteIndex] &= ~ourBitMask;
            }
        }
    }
}

internal uint64 getMicroseconds_() {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return res.tv_sec * 1000 * 1000 + res.tv_nsec / 1000;
}

#define DO_GLX_LOAD_FUNCTION(name, returnType, ...) \
    gl-> name = (gl##name##Fn*) glXGetProcAddress((const GLubyte*) "gl" #name); \
    if (!gl-> name) {                                                   \
        fprintf(stderr, "Could not load OpenGL function gl" #name);     \
    }                                                                   

#define DO_GLX_LOAD_FUNCTION_LOCAL(name, returnType, ...)               \
    gl##name##Fn* gl##name = (gl##name##Fn*) glXGetProcAddress((const GLubyte*) "gl" #name);

internal void loadOpenGLFunctions(OpenGLFunctions* gl) {
    FOR_EACH_GL_FUNCTION(DO_GLX_LOAD_FUNCTION)
}

internal bool32 isGLXExtensionPresent(Display* dpy, int screen, const char* extensionName) {
    const char* extensionString = glXQueryExtensionsString(dpy, screen);
    const char* cursor = extensionString;

    do {
        while (*extensionName && *cursor++ == *extensionName++)
            ;

        if (!*extensionName) {
            return true;
        }
            
        while (*cursor && *cursor != ' ') {
            cursor++;
        }

        if (*cursor == ' ') {
            cursor++;
        }
    } while(*cursor);
        
    return false;
}

internal void resetProgramMemory_(ProgramMemory* memory);

internal void initializeProgramMemory(ProgramMemory* memory) {
    memory->permanentStorageSize = MEGABYTES(64);
    memory->transientStorageSize = GIGABYTES(2);

    memory->permanentStorage = mmap(0,
                                    memory->permanentStorageSize,
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE,
                                    -1,
                                    0);
    memory->transientStorage = mmap(0,
                                    memory->transientStorageSize,
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE,
                                    -1,
                                    0);
    ASSERT(memory->transientStorage
           && memory->permanentStorage);

    memory->platform.readFileIntoMemory = &readFileIntoMemory_;
    memory->platform.getFileSize = &getFileSize_;
    memory->platform.getMicroseconds = &getMicroseconds_;
    memory->platform.outputBufferInConsole = &outputBufferInConsole_;
    memory->platform.resetProgramMemory = &resetProgramMemory_;
    loadOpenGLFunctions(&memory->gl);
}

internal void resetProgramMemory_(ProgramMemory* memory) {
    int err = munmap(memory->permanentStorage,
                     memory->permanentStorageSize);
    ASSERT(!err);
    
    err = munmap(memory->transientStorage,
                 memory->transientStorageSize);
    ASSERT(!err);

    *memory = (ProgramMemory){};
    initializeProgramMemory(memory);
}

#define MAX_INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define INOTIFY_BUFFER_SIZE MAX_INOTIFY_EVENT_SIZE * 256

#define FOR_ALL_NEEDED_GLX_FUNCTIONS(X) \
    X(XSwapIntervalEXT, void, Display*, GLXDrawable, int)                \
    X(XCreateContextAttribsARB, GLXContext, Display*, GLXFBConfig, GLXContext, Bool, const int*)

FOR_ALL_NEEDED_GLX_FUNCTIONS(DO_GL_FUNCTION_TYPEDEF)

/* // aim for 60 FPS */
/* #define MANUAL_SYNC_FRAME_DURATION 16666 */

// rely on V-sync
#define MANUAL_SYNC_FRAME_DURATION 0

// TODO(octave) : use sigaction instead
void signalHandler(int signum) {
    switch (signum) {
    case SIGSEGV:
        fprintf(stderr, "Segmentation fault.\n");
        break;
    default:
        fprintf(stderr, "Unknown signal received.\n");
    }

    void* backtraceBuffer[1024];
    
    int depth = backtrace(backtraceBuffer, ARRAY_COUNT(backtraceBuffer));
    char** backtraceSymbols = backtrace_symbols(backtraceBuffer, depth);

    fprintf(stderr, "Backtrace : \n");

    // NOTE(octave) : skipping (0) this function and
    //                         (1) the libc function that caught the signal
    for (int i = 2; i < depth; i++) {
        fprintf(stderr, "  [%d] %s\n", i-2, backtraceSymbols[i]);
    }

    free(backtraceSymbols);

    exit(1);
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    signal(SIGSEGV, signalHandler);

    FOR_ALL_NEEDED_GLX_FUNCTIONS(DO_GLX_LOAD_FUNCTION_LOCAL);


    LinuxContext ctx = {};

    uint32 initialWindowWidth = 640;
    uint32 initialWindowHeight = 288 * 2;
    uint32 bytes_per_pixel = 4;

    ctx.dpy = XOpenDisplay(NULL);
    if (!ctx.dpy) {
        fprintf(stderr, "No X11 display available.\n");
        return 1;
    }

    int default_screen = XDefaultScreen(ctx.dpy);

    XVisualInfo vinfo;
    GLXFBConfig glxConfig;
    {
        int errorBase;
        int eventBase;
        if (!glXQueryExtension(ctx.dpy, &errorBase, &eventBase)) {
            fprintf(stderr, "No GLX extension on this X server connection, exiting.\n");
            return 1;
        }

        int major, minor;
        if (!glXQueryVersion(ctx.dpy, &major, &minor)) {
            fprintf(stderr, "Could not query GLX version, exiting.\n");
            return 1;
        }

        if (major < 1 || minor < 1) {
            fprintf(stderr, "GLX version 1.1 or later is required but %d.%d was found, exiting.\n", major, minor);
            return 1;
        }

        int requiredConfigAttributes[] = {
            GLX_X_RENDERABLE, true,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_BUFFER_SIZE, 32,
            GLX_DOUBLEBUFFER, true,
            None,
        };

        int matchingConfigCount;
        GLXFBConfig* availableConfigs = glXChooseFBConfig(ctx.dpy,
                                                          default_screen,
                                                          requiredConfigAttributes,
                                                          &matchingConfigCount);
        if (!matchingConfigCount) {
            fprintf(stderr, "Could not find a suitable GLX config, exiting.\n");
            return 1;
        }

        glxConfig = availableConfigs[0];

        XVisualInfo* pVinfo = glXGetVisualFromFBConfig(ctx.dpy, glxConfig);
        ASSERT(pVinfo);
        vinfo = *pVinfo;
    }

    Window root_window = XDefaultRootWindow(ctx.dpy);

    XSetWindowAttributes window_attributes;
    window_attributes.background_pixel = 0;
    window_attributes.colormap = XCreateColormap(ctx.dpy, root_window, vinfo.visual, AllocNone);
    window_attributes.event_mask =
        StructureNotifyMask
        | KeyPressMask
        | KeyReleaseMask
        | PointerMotionMask
        | ButtonPressMask
        | ButtonReleaseMask;
    window_attributes.bit_gravity = StaticGravity;

    ctx.window =
        XCreateWindow(ctx.dpy, root_window, 0, 0, initialWindowWidth,
                      initialWindowHeight, 0, vinfo.depth, InputOutput, vinfo.visual,
                      CWBackPixel | CWColormap | CWEventMask | CWBitGravity,
                      &window_attributes);

    if (!ctx.window) {
        fprintf(stderr, "Could not create window, exiting.\n");
        return 1;
    }

    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "Could not find glXCreateContextAttribsARB function, exiting.\n");
        return 1;
    }

    GLXWindow glxWindow = glXCreateWindow(ctx.dpy, glxConfig, ctx.window, 0);

    // NOTE(octave) : using OpenGL 3.3 core
    int glxContextAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None,
    };
    GLXContext glxContext = glXCreateContextAttribsARB(ctx.dpy,
                                                       glxConfig,
                                                       0,
                                                       true,
                                                       glxContextAttribs);

    if (!glxContext) {
        fprintf(stderr, "Could not create GLX context, exiting.\n");
        return 1;
    }

    ASSERT(glXIsDirect(ctx.dpy, glxContext));

    if (!glXMakeContextCurrent(ctx.dpy, glxWindow, glxWindow, glxContext)) {
        fprintf(stderr, "Could not make GLX context current, exiting.\n");
        return 1;
    }
    
    if (isGLXExtensionPresent(ctx.dpy, default_screen, "GLX_EXT_swap_control")) {
        glXSwapIntervalEXT(ctx.dpy, glxWindow, -1);
    } else {
        fprintf(stderr, "Cannot find extension GLX_EXT_swap_control, V-sync will be off\n");
    }
        
    XStoreName(ctx.dpy, ctx.window, "Handmade");
    XMapWindow(ctx.dpy, ctx.window);

    // Keyboard input context setup (used for text decoding)
    {
        XIM x_input_method = XOpenIM(ctx.dpy, NULL, NULL, NULL);
        if (!x_input_method) {
            fprintf(stderr, "Could not open input method.\n");
        }

        XIMStyles* styles = NULL;
        if (XGetIMValues(x_input_method, XNQueryInputStyle, &styles, NULL) || !styles) {
            fprintf(stderr, "Could not retrieve input styles.\n");
        }

        XIMStyle best_match_style = 0;
        for (int i = 0; i < styles->count_styles; i++) {
            XIMStyle this_style = styles->supported_styles[i];

            if (this_style == (XIMPreeditNothing | XIMStatusNothing)) {
                best_match_style = this_style;
                break;
            }
        }
        XFree(styles);

        if (!best_match_style) {
            fprintf(stderr, "Could not find matching input style.\n");
        }

        ctx.inputContext = XCreateIC(x_input_method,
                                  XNInputStyle, best_match_style,
                                  XNClientWindow, ctx.window,
                                  XNFocusWindow, ctx.window,
                                  NULL);
        if (!ctx.inputContext) {
            fprintf(stderr, "Could not create input context\n");
        }
    }

    XFlush(ctx.dpy);

    InputInfo inputInfo = {};

    WM_DELETE_WINDOW = XInternAtom(ctx.dpy, "WM_DELETE_WINDOW", true);
    if (!XSetWMProtocols(ctx.dpy, ctx.window, &WM_DELETE_WINDOW, 1)) {
        fprintf(stderr, "Could not register WM_DELETE_WINDOW.\n");
    }

    ctx.x11_backbuffer.width = 640;
    ctx.x11_backbuffer.height = 480;
    ctx.x11_backbuffer.xoffset = 0;
    ctx.x11_backbuffer.format = ZPixmap;
    ctx.x11_backbuffer.byte_order = LSBFirst;
    ctx.x11_backbuffer.bitmap_unit = bytes_per_pixel * 8;
    ctx.x11_backbuffer.bitmap_bit_order = LSBFirst;
    ctx.x11_backbuffer.bitmap_pad = bytes_per_pixel * 8;
    ctx.x11_backbuffer.depth = vinfo.depth;
    ctx.x11_backbuffer.bytes_per_line = initialWindowWidth * bytes_per_pixel;
    ctx.x11_backbuffer.bits_per_pixel = bytes_per_pixel * 8;

    ctx.x11_backbuffer.data = (char*)calloc(ctx.x11_backbuffer.bytes_per_line * ctx.x11_backbuffer.height * bytes_per_pixel, 1);

    OffscreenBuffer backbuffer = copy_x11_backbuffer(ctx.x11_backbuffer);

    int inotify_fd = inotify_init1(IN_NONBLOCK);
    int lib_handmade_watch = inotify_add_watch(inotify_fd, "./", IN_CLOSE_WRITE);

    if (lib_handmade_watch < 0) {
        // TODO(octave): Logging.
        return 1;
    }

    void* lib_handmade_handle = 0;
    UpdateProgramAndRenderFunction* update_program_and_render = 0;
    bool32 need_handmade_lib_reload = true;

    uint8 inotifyBuffer[INOTIFY_BUFFER_SIZE];

    ProgramMemory programMemory = {};
    initializeProgramMemory(&programMemory);

    uint64 tprev = getMicroseconds_();
    (void)tprev;
    
    // main loop
    for (;;) {
        // read inotify events
        for (;;) {
            int bytes_read = read(inotify_fd, inotifyBuffer, INOTIFY_BUFFER_SIZE);
            if (bytes_read == -1) {
                if (errno == EAGAIN) {
                    break;
                } else {
                    return 1;
                }
            }

            int i = 0;
            while (i < bytes_read) {
                struct inotify_event* event = (struct inotify_event*) &inotifyBuffer[i];

                if (!strcmp(event->name, "libhandmade.so")
                    && (event->mask & IN_CLOSE_WRITE)) {
                    need_handmade_lib_reload = true;
                }

                i += sizeof(struct inotify_event) + event->len;
            }
        }

        if (need_handmade_lib_reload) {
            if (lib_handmade_handle) {
                dlclose(lib_handmade_handle);
            }
            lib_handmade_handle = dlopen("./libhandmade.so", RTLD_NOW);

            if (!lib_handmade_handle) {
                fprintf(stderr, "%s\n", dlerror());
                // TODO(octave) : better error handling
                return 1;
            }

            update_program_and_render =
                (UpdateProgramAndRenderFunction*)dlsym(lib_handmade_handle, "updateProgramAndRender");

            if (!update_program_and_render) {
                fprintf(stderr, "%s\n", dlerror());
                // TODO(octave) : better error handling
                return 1;
            }
            need_handmade_lib_reload = false;
        }

        // read events
        processInputEvents(&ctx, &backbuffer, &inputInfo);
        if (ctx.shouldExit) {
            break;
        }

        if (update_program_and_render(&programMemory, &inputInfo)) {
            break;
        }
        inputInfo.eventCount = 0;

        glXSwapBuffers(ctx.dpy, glxWindow);

#if MANUAL_SYNC_FRAME_DURATION > 0
        uint64 tnow = getMicroseconds_();
        int64 excess = (tprev + MANUAL_SYNC_FRAME_DURATION) - tnow;
        if (excess < 0) {
            fprintf(stderr, "Warning : Missed frame deadline by %fms.\n", -(float)excess / (1000.0f));
        } else {
            int err = usleep(excess);
            ASSERT(!err);
        }

        tprev = tnow;
#endif
    }

    return 0;
}
