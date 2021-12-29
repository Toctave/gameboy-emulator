#include "handmade.h"

UPDATE_PROGRAM_AND_RENDER(updateProgramAndRender) {
    memory->gl.ClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    memory->gl.Clear(GL_COLOR_BUFFER_BIT);
    
    return false;
}
