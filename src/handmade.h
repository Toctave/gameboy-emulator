#pragma once

#define HANDMADE_INTERNAL

#include <stdint.h>
#include "handmade_types.h"
#include "handmade_opengl.h"
#include "handmade_keyboard.h"

#ifdef HANDMADE_INTERNAL
#include <stdio.h>
#include <signal.h>

#define ASSERT(expr)                                \
    if (!(expr)) {                                  \
        fprintf(stderr,                             \
                "%s:%d : Assertion '%s' failed.\n", \
                __FILE__, __LINE__, #expr);         \
        raise(SIGTRAP);                             \
    }

#define ASSERT_MSG(expr, ...)                               \
    if (!(expr)) {                                          \
        fprintf(stderr,                                     \
                "%s:%d : Assertion '%s' : \n    Message : \"",   \
                __FILE__, __LINE__, #expr);                 \
        fprintf(stderr, __VA_ARGS__);                       \
        fprintf(stderr, "\"\n");                              \
        raise(SIGTRAP);                                     \
    }

#else
#define ASSERT(expr)
#define ASSERT_MSG(expr, message)
#endif

#define DO_ENUMERAND_NAME_WITH_VALUE(name, value) #name,
#define DO_ENUMERAND_WITH_VALUE(name, value) name = value,
#define DECLARE_ENUM_WITH_VALUES(name, type, forAllItems)       \
    typedef enum name {                                         \
        forAllItems(DO_ENUMERAND_WITH_VALUE)                    \
    } name;                                                     \
    global const char* name##Names[] = {                        \
        forAllItems(DO_ENUMERAND_NAME_WITH_VALUE)               \
    };                                                          \
    global const uint32 name##Count = ARRAY_COUNT(name##Names);

#define DO_ENUMERAND_NAME(name) #name,
#define DO_ENUMERAND(name) name,
#define DECLARE_ENUM(name, type, forAllItems)                   \
    typedef enum name {                                         \
        forAllItems(DO_ENUMERAND)                               \
    } name;                                                     \
    global const char* name##Names[] = {                        \
        forAllItems(DO_ENUMERAND_NAME)                          \
    };                                                          \
    global const uint32 name##Count = ARRAY_COUNT(name##Names);

#define global static
#define local_persist static
#define internal

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(*array))

#define KILOBYTES(value) ((value) * (uint64)1024)
#define MEGABYTES(value) (KILOBYTES(value) * (uint64)1024)
#define GIGABYTES(value) (MEGABYTES(value) * (uint64)1024)

typedef struct OffscreenBuffer {
    uint32 width;
    uint32 height;

    // NOTE(octave): 32 bits per pixel, where the low-order byte is
    // blue, followed by green, then red, then nothing for the
    // high-order byte
    uint32* data;
} OffscreenBuffer;

#define MOUSEBUTTON_ITEMS(ITEM)                   \
    ITEM(MOUSEBUTTON_LEFT, 0x01)                  \
    ITEM(MOUSEBUTTON_MIDDLE, 0x02)                \
    ITEM(MOUSEBUTTON_RIGHT, 0x04)                 \
    ITEM(MOUSEBUTTON_COUNT, 3)

DECLARE_ENUM_WITH_VALUES(MouseButton, uint8, MOUSEBUTTON_ITEMS)

typedef struct MouseState {
    int32 x;
    int32 y;
    uint8 buttonsDown : MOUSEBUTTON_COUNT;
} MouseState;

typedef struct KeyboardState {
    uint64 keysDown[4];
} KeyboardState;

#define EVENTTYPE_ITEMS(ITEM)                   \
    ITEM(EVENT_KEY)                             \
    ITEM(EVENT_MOUSEMOVE)                       \
    ITEM(EVENT_MOUSEBUTTON)                     \
    ITEM(EVENT_MOUSEWHEEL)                      \

DECLARE_ENUM(InputEventType, uint8, EVENTTYPE_ITEMS)

typedef enum PressFlag {
    PRESS,
    RELEASE,
    REPEAT,
} PressFlag;

// TODO(octave) : add timestamp ?
typedef struct InputEvent {
    InputEventType type;
    
    struct {
        PressFlag pressFlag;
        KeyIndex index;
        uint8 utf8[4]; // should start with a 0 if not applicable
    } key;

    struct {
        int32 x;
        int32 y;
        int32 xPrev;
        int32 yPrev;
    } mouseMove;

    struct {
        int32 x;
        int32 y;
        PressFlag flag;
        MouseButton button;
    } mouseButton;

    struct {
        int32 scrollAmount;
    } mouseWheel;
} InputEvent;

#define MAX_EVENTS_PER_FRAME 256

typedef struct InputState {
    KeyboardState keyboard;
    MouseState mouse;

    uint32 windowWidth;
    uint32 windowHeight;
} InputState;

typedef struct InputInfo {
    int argc;
    char** argv;
    
    uint32 eventCount;
    InputEvent events[MAX_EVENTS_PER_FRAME];
    bool32 didResize;

    InputState state;
} InputInfo;

struct ProgramMemory;

typedef struct PlatformFunctions {
    bool32 isInitialized;
    uint64 (*getFileSize)(const char* filepath, bool32* success);
    bool32 (*readFileIntoMemory)(const char* filepath, void* buffer, uint64 size);
    uint64 (*getMicroseconds)(void);
    void (*outputBufferInConsole)(uint8* buffer, uint64 size);
    void (*resetProgramMemory)(struct ProgramMemory* memory);
    void (*setCursor)(uint32 index);
} PlatformFunctions;

typedef struct ProgramMemory {
    bool32 isInitialized;
    PlatformFunctions platform;
    OpenGLFunctions gl;
    
    uint64 transientStorageSize;
    void* transientStorage;

    uint64 permanentStorageSize;
    void* permanentStorage;
} ProgramMemory;

bool32 isKeyDown(InputInfo* input, KeyIndex key);

#define UPDATE_PROGRAM_AND_RENDER(name) bool32 name(ProgramMemory* memory, InputInfo* input)

typedef UPDATE_PROGRAM_AND_RENDER(UpdateProgramAndRenderFunction);

UPDATE_PROGRAM_AND_RENDER(update_program_and_render);

extern PlatformFunctions platform;
extern OpenGLFunctions gl;
