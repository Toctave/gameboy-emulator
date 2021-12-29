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
} ProgramState;

UPDATE_PROGRAM_AND_RENDER(updateProgramAndRender) {
    if (!platform.isInitialized) {
        platform = memory->platform;
    }
    
    if (!gl.isInitialized) {
        gl = memory->gl;
        gl.Enable(GL_DEBUG_OUTPUT);
    }

    ProgramState* state = memory->permanentStorage;
    if (!state->isInitialized) {
        initializeMemoryArena(&state->transientArena,
                              memory->transientStorageSize,
                              memory->transientStorage);
        initializeMemoryArena(&state->permanentArena,
                              memory->permanentStorageSize - sizeof(ProgramState),
                              (uint8*)memory->transientStorage + sizeof(ProgramState));
        const char* vertexShaderSource =
            "in vec2 position;\n"
            "\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 0, 1);\n"
            "}\n";

        const char* fragmentShaderSource =
            "in vec4 vertexColor;\n"
            "\n"
            "out vec4 outColor;\n"
            "\n"
            "void main() {\n"
            "    outColor = vec4(1, 1, 1, 1);"
            "}\n";
    
        state->shader = createShaderProgramFromSources(&state->transientArena,
                                                       vertexShaderSource,
                                                       fragmentShaderSource);

        ASSERT(state->shader);
        state->isInitialized = true;
    }
    
    gl.DebugMessageCallback(openglDebugCallback, NULL);

    gl.ClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);

    uint32 vbo;
    gl.GenBuffers(1, &vbo);

    uint32 vao;
    gl.GenVertexArrays(1, &vao);

    float vertexData[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BindVertexArray(vao); 
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0,
                           2,
                           GL_FLOAT,
                           GL_FALSE,
                           0,
                           0);
    
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

    gl.UseProgram(state->shader);

    gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    return false;
}
