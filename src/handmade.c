#include "handmade.h"
#include "handmade_memory.h"

#include <stdio.h>

PlatformFunctions platform;
OpenGLFunctions gl;

void openglDebugCallback(GLenum source,
                         GLenum type,
                         GLuint id,
                         GLenum severity,
                         GLsizei length,
                         const GLchar* message,
                         const void* userParam) {
    fprintf(stderr, "%s\n", message);
}

internal GLuint buildGLSLShaderSource(MemoryArena* arena,
                                      GLenum shaderStage,
                                      const char* source) {
    uint64 prevUsed = arena->used;

    const char* prelude =
        "#version 330\n";

    const char* sources[] = {
        prelude,
        source,
    };
    
    GLuint shader = gl.CreateShader(shaderStage);

    gl.ShaderSource(shader, ARRAY_COUNT(sources), sources, 0);

    gl.CompileShader(shader);

    GLint status;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint length;
        gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        char* infoLog = pushArray(arena, length, char);
        gl.GetShaderInfoLog(shader, length, 0, infoLog);

        printf("Error when compiling shader :\n%s\n", infoLog);

        gl.DeleteShader(shader);
        shader = 0;
    }

    arena->used = prevUsed;
    return shader;
}

internal GLuint linkVertexAndFragmentShader(MemoryArena* arena,
                                            GLuint vs,
                                            GLuint fs) {
    if (vs && fs) {
        GLuint program = gl.CreateProgram();
        gl.AttachShader(program, vs);
        gl.AttachShader(program, fs);

        gl.LinkProgram(program);

        GLint status;
        gl.GetProgramiv(program, GL_LINK_STATUS, &status);

        if (status) {
            return program;
        } else {
            GLint length;
            gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

            char* infoLog = pushArray(arena, length, char);
            gl.GetProgramInfoLog(program, length, 0, infoLog);
            
            printf("Error when linking shaders :\n%s\n", infoLog);

            gl.DeleteProgram(program);
            return 0;
        }
    } else {
        return 0;
    }
}

internal GLuint createShaderProgramFromSources(MemoryArena* arena,
                                               const char* vertexSource,
                                               const char* fragmentSource) {
    uint64 prevUsed = arena->used;
    GLuint vs = buildGLSLShaderSource(arena, GL_VERTEX_SHADER, vertexSource);
    GLuint fs = buildGLSLShaderSource(arena, GL_FRAGMENT_SHADER, fragmentSource);

    GLuint program = linkVertexAndFragmentShader(arena, vs, fs);
    
    arena->used = prevUsed;
    return program;
}

typedef struct ProgramState {
    bool32 isInitialized;
    
    MemoryArena transientArena;
    MemoryArena permanentArena;

    uint32 shader;
    uint32 vao;
    uint32 vbo;

    uint32 texture;
} ProgramState;

UPDATE_PROGRAM_AND_RENDER(updateProgramAndRender) {
    if (!platform.isInitialized) {
        platform = memory->platform;
    }
    
    if (!gl.isInitialized) {
        gl = memory->gl;
        gl.Enable(GL_DEBUG_OUTPUT);
        gl.DebugMessageCallback(openglDebugCallback, NULL);
    }

    ProgramState* state = memory->permanentStorage;
    uint32 width = 4;
    uint32 height = 2;

    if (!state->isInitialized) {
        // Memory arenas
        initializeMemoryArena(&state->transientArena,
                              memory->transientStorageSize,
                              memory->transientStorage);
        initializeMemoryArena(&state->permanentArena,
                              memory->permanentStorageSize - sizeof(ProgramState),
                              (uint8*)memory->transientStorage + sizeof(ProgramState));

        // Shader
        const char* vertexShaderSource =
            "in vec2 position;\n"
            "out vec2 vertexUV;\n"
            "\n"
            "void main() {\n"
            "    gl_Position = vec4(2 * position - 1, 0, 1);\n"
            "    vertexUV = position;\n"
            "}\n";

        const char* fragmentShaderSource =
            "uniform sampler2D tex;\n"
            "\n"
            "in vec2 vertexUV;\n"
            "\n"
            "out vec4 outColor;\n"
            "\n"
            "void main() {\n"
            "    outColor.a = 1;\n"
            "    outColor.rgb = vec3(texture(tex, vertexUV).r);\n"
            "}\n";
    
        state->shader = createShaderProgramFromSources(&state->transientArena,
                                                       vertexShaderSource,
                                                       fragmentShaderSource);

        ASSERT(state->shader);

        // Vertex data
        gl.GenBuffers(1, &state->vbo);
        gl.GenVertexArrays(1, &state->vao);

        float vertexData[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
        };

        gl.BindBuffer(GL_ARRAY_BUFFER, state->vbo);
        gl.BindVertexArray(state->vao); 
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(0,
                               2,
                               GL_FLOAT,
                               GL_FALSE,
                               0,
                               0);
    
        gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

        gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        gl.BindVertexArray(0);

        // Texture
        gl.GenTextures(1, &state->texture);
        gl.BindTexture(GL_TEXTURE_2D, state->texture);

        gl.TexImage2D(GL_TEXTURE_2D,
                      0,
                      GL_RGBA,
                      width,
                      height,
                      0,
                      GL_RED,
                      GL_UNSIGNED_BYTE,
                      NULL);

        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
        gl.BindTexture(GL_TEXTURE_2D, 0);
        
        int32 texLoc = gl.GetUniformLocation(state->shader, "tex");
        ASSERT(texLoc >= 0);

        gl.UseProgram(state->shader);
        gl.Uniform1i(texLoc, 0);
        gl.UseProgram(state->shader);
        
        state->isInitialized = true;
    }

#if true
    for (uint32 eventIndex = 0; eventIndex < input->eventCount; eventIndex++) {
        InputEvent* event = &input->events[eventIndex];

        if (event->type == EVENT_KEY
            && event->key.pressFlag == PRESS
            && event->key.index == KID_F5) {
            platform.resetProgramMemory(memory);
            return false;
        }
    }
#endif
    
    gl.ClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    
    static uint8 i = 0;
    i += 4;

    uint8 textureData[] = {
        i, 255 - i, i, 255 - i,
        255 - i, i, 255 - i, i
    };

    ASSERT(ARRAY_COUNT(textureData) == width * height);

    gl.BindTexture(GL_TEXTURE_2D, state->texture);
    gl.BindBuffer(GL_ARRAY_BUFFER, state->vbo);
    gl.BindVertexArray(state->vao); 
    gl.UseProgram(state->shader);
    
    gl.TexSubImage2D(GL_TEXTURE_2D,
                     0,
                     0, 0,
                     width, height,
                     GL_RED, GL_UNSIGNED_BYTE,
                     textureData);

    gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    gl.BindTexture(GL_TEXTURE_2D, 0);
    gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    gl.BindVertexArray(0); 
    gl.UseProgram(0);
    
    return false;
}
