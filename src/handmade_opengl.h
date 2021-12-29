#pragma once

#include <GL/gl.h>
#include "handmade_types.h"

#define DO_GL_FUNCTION_TYPEDEF(name, returnType, ...)   \
    typedef returnType gl##name##Fn(__VA_ARGS__);

#define DO_GL_FUNCTION_STRUCT_MEMBER(name, returnType, ...) \
    gl##name##Fn* name;

// NOTE(octave) : sorted alphabetically
#define FOR_EACH_GL_FUNCTION(X)                                         \
    X(ActiveTexture, void, GLenum)                                      \
    X(AttachShader, void, GLuint, GLuint)                               \
    X(BindBuffer, void, GLenum, GLuint)                                 \
    X(BindTexture, void, GLenum, GLuint)                                \
    X(BindVertexArray, void, GLuint)                                    \
    X(BlendFunc, void, GLenum, GLenum)                                  \
    X(BufferData, void, GLenum, GLsizeiptr, const GLvoid *, GLenum)     \
    X(BufferSubData, void, GLenum, GLintptr, GLsizeiptr, const void*)   \
    X(Clear, void, GLbitfield)                                          \
    X(ClearColor, void, GLclampf, GLclampf, GLclampf, GLclampf)         \
    X(CompileShader, void, GLuint)                                      \
    X(CreateProgram, GLuint)                                            \
    X(CreateShader, GLuint, GLenum)                                     \
    X(DebugMessageCallback, void, GLDEBUGPROC, void*)                   \
    X(DeleteProgram, void, GLuint)                                      \
    X(DeleteShader, void, GLuint)                                       \
    X(Disable, void, GLenum)                                            \
    X(DrawArrays, void, GLenum, GLint, GLsizei)                         \
    X(DrawElements, void, GLenum, GLsizei, GLenum, const void*)         \
    X(Enable, void, GLenum)                                             \
    X(EnableVertexAttribArray, void, GLuint)                            \
    X(Finish, void, void)                                               \
    X(GenBuffers, void, GLsizei, GLuint*)                               \
    X(GenTextures, void, GLsizei, GLuint*)                              \
    X(GenVertexArrays, void, GLsizei, GLuint*)                          \
    X(GetFloatv, void, GLenum, GLfloat*)                                \
    X(GetIntegerv, void, GLenum, GLint*)                                \
    X(GetProgramInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*)      \
    X(GetProgramiv, void, GLuint, GLenum, GLint*)                       \
    X(GetShaderInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*)       \
    X(GetShaderiv, void, GLuint, GLenum, GLint*)                        \
    X(GetTexImage, void, GLenum, GLint, GLenum, GLenum, GLvoid*)        \
    X(GetUniformLocation, GLint, GLuint, const GLchar*)                 \
    X(LinkProgram, void, GLuint)                                        \
    X(Scissor, void, GLint, GLint, GLsizei, GLsizei)                    \
    X(ShaderSource, void, GLuint, GLsizei, const GLchar**, const GLint*)\
    X(TexImage2D, void, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) \
    X(TexParameteri, void, GLenum, GLenum, GLint)                       \
    X(Uniform1f, void, GLint, GLfloat)                                  \
    X(Uniform1fv, void, GLint, GLsizei, const GLfloat*)                 \
    X(Uniform1i, void, GLint, GLint)                                    \
    X(Uniform1iv, void, GLint, GLsizei, const GLint*)                   \
    X(Uniform1ui, void, GLuint, GLuint)                                 \
    X(Uniform1uiv, void, GLint, GLsizei, const GLuint*)                 \
    X(Uniform2f, void, GLint, GLfloat, GLfloat)                         \
    X(Uniform2fv, void, GLint, GLsizei, const GLfloat*)                 \
    X(Uniform2i, void, GLint, GLint, GLint)                             \
    X(Uniform2iv, void, GLint, GLsizei, const GLint*)                   \
    X(Uniform2ui, void, GLuint, GLuint, GLuint)                         \
    X(Uniform2uiv, void, GLint, GLsizei, const GLuint*)                 \
    X(Uniform3f, void, GLint, GLfloat, GLfloat, GLfloat)                \
    X(Uniform3fv, void, GLint, GLsizei, const GLfloat*)                 \
    X(Uniform3i, void, GLint, GLint, GLint, GLint)                      \
    X(Uniform3iv, void, GLint, GLsizei, const GLint*)                   \
    X(Uniform3ui, void, GLuint, GLuint, GLuint, GLuint)                 \
    X(Uniform3uiv, void, GLint, GLsizei, const GLuint*)                 \
    X(Uniform4f, void, GLint, GLfloat, GLfloat, GLfloat, GLfloat)       \
    X(Uniform4fv, void, GLint, GLsizei, const GLfloat*)                 \
    X(Uniform4i, void, GLint, GLint, GLint, GLint, GLint)               \
    X(Uniform4iv, void, GLint, GLsizei, const GLint*)                   \
    X(Uniform4ui, void, GLuint, GLuint, GLuint, GLuint, GLuint)         \
    X(Uniform4uiv, void, GLint, GLsizei, const GLuint*)                 \
    X(UniformMatrix2fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix2x3fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix2x4fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix3fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix3x2fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix3x4fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix4fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix4x2fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UniformMatrix4x3fv, void, GLint, GLsizei, GLboolean, const GLfloat*) \
    X(UseProgram, void, GLuint)                                         \
    X(VertexAttribPointer, void, GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid*) \
    X(Viewport, void, GLint, GLint, GLsizei, GLsizei)                   \

FOR_EACH_GL_FUNCTION(DO_GL_FUNCTION_TYPEDEF)

typedef struct OpenGLFunctions {
    bool32 isInitialized;
    FOR_EACH_GL_FUNCTION(DO_GL_FUNCTION_STRUCT_MEMBER);
} OpenGLFunctions;
