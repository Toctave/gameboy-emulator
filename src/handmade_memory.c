#include "handmade_memory.h"

#include "handmade.h"

MemoryArenaMarker getMarker(MemoryArena* arena) {
    return (MemoryArenaMarker){arena->used};
}

void freeToMarker(MemoryArena* arena, MemoryArenaMarker marker) {
    arena->used = marker.used;
}

uint8* pushSize_(MemoryArena* arena, uint64 size) {
    ASSERT(arena->used + size <= arena->size);
    
    uint8* result = arena->base + arena->used;

    arena->used += size;
    
    return result;
}

#define pushBinaryFile(arena, filepath, sizeOut) pushFile_(arena, filepath, false, sizeOut)
#define pushTextFile(arena, filepath, sizeOut) (char*)pushFile_(arena, filepath, true, sizeOut)
uint8* pushFile_(MemoryArena* arena, const char* filepath, bool32 nullTerminate, uint64* fileSizeOut) {
    MemoryArenaMarker beforeUse = getMarker(arena);
    
    bool32 success;
    uint64 fileSize = platform.getFileSize(filepath, &success);

    if (!success) {
        return 0;
    }

    if (fileSizeOut) {
        *fileSizeOut = fileSize;
    }
    
    if (nullTerminate) fileSize++;
    
    uint8* buffer = pushSize_(arena, fileSize);

    if (!platform.readFileIntoMemory(filepath, buffer, fileSize)) {
        freeToMarker(arena, beforeUse);
        
        return 0;
    }

    if (nullTerminate) buffer[fileSize-1] = 0;

    return buffer;
}

void initializeMemoryArena(MemoryArena* arena, uint64 size, uint8* base) {
    arena->base = base;
    arena->size = size;
    arena->used = 0;
}

void initializeSubArena(MemoryArena* arena, MemoryArena* parent, uint64 size) {
    arena->base = pushArray(parent, size, uint8);
    arena->size = size;
    arena->used = 0;
}

void resetMemoryArena(MemoryArena* arena) {
    arena->used = 0;
}

