#include "handmade.h"

#include <stdio.h>

global PlatformFunctions platform;
global OpenGLFunctions gl;

UPDATE_PROGRAM_AND_RENDER(updateProgramAndRender) {
    if (!platform.isInitialized) {
        platform = memory->platform;
    }
    
    if (!gl.isInitialized) {
        gl = memory->gl;
    }
    
    gl.ClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    
    return false;
}
