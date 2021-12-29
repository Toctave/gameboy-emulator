#pragma once

#include "handmade_types.h"

typedef struct MemoryArena {
    uint8* base;
    uint64 size;
    uint64 used;
} MemoryArena;

typedef struct MemoryArenaMarker {
    uint64 used;
} MemoryArenaMarker;

#define pushOne(arena, type) (type*)pushSize_(arena, sizeof(type))
#define pushArray(arena, count, type) (type*)pushSize_(arena, count * sizeof(type))
uint8* pushSize_(MemoryArena* arena, uint64 size);

void initializeMemoryArena(MemoryArena* arena, uint64 size, uint8* base);
void initializeSubArena(MemoryArena* arena, MemoryArena* parent, uint64 size);
void resetMemoryArena(MemoryArena* arena);

MemoryArenaMarker getMarker(MemoryArena* arena);
void freeToMarker(MemoryArena* arena, MemoryArenaMarker marker);

