#include "gameboy.h"

#include <stdio.h>

uint8 grayLevel(uint8 palette, uint8 id) {
    uint8 color = (palette >> (id * 2)) & 0x3;

    uint8 shades[4] = {0, 85, 170, 255};
    return shades[color];
}

void assembleTileLine(GameBoy* gb, uint8 palette, uint8* source, uint8* destination) {
    for (uint8 x = 0; x < 8; x++) {
        uint8 id_lsb = (source[0] >> (7 - x)) & 1;
        uint8 id_msb = (source[1] >> (7 - x)) & 1;

        uint8 id = (id_msb << 1) | id_lsb;
        uint8 gray = grayLevel(palette, id);

        *destination++ = gray;
    }
}

uint8* getBGWindowTile(GameBoy* gb, uint8 tileIndex) {
    uint8 lcdc = MEM(MMR_LCDC);

    if (getBit(lcdc, 4)) {
        return &MEM(TILE_DATA_BLOCK0 + tileIndex * 16);
    } else {
        if (tileIndex < 128) {
            return &MEM(TILE_DATA_BLOCK2 + tileIndex * 16);
        } else {
            return &MEM(TILE_DATA_BLOCK1 + (tileIndex - 128) * 16);
        }
    }
}

void drawBackground(GameBoy* gb) {
    uint8 palette = MEM(MMR_BGP);
    uint8 scx = MEM(MMR_SCX);
    uint8 scy = MEM(MMR_SCY);
    uint8 lcdc = MEM(MMR_LCDC);

    uint8* tileMap = getBit(lcdc, 3) ? &MEM(TILEMAP1) : &MEM(TILEMAP0);

    uint8* currentTilePtr = tileMap;
    for (uint8 tileY = 0; tileY < 32; tileY++) {
        for (uint8 tileX = 0; tileX < 32; tileX++) {
            uint8 tileIndex = *currentTilePtr++;

            uint8* tile = getBGWindowTile(gb, tileIndex);

            for (uint8 row = 0; row < 8; row++) {
                uint8 screenRow = tileY * 8 + row;
                if (screenRow < GAMEBOY_SCREEN_HEIGHT
                    && tileX * 8 + 7 < GAMEBOY_SCREEN_WIDTH) {
                    uint8* dst = &gb->screen[screenRow][tileX * 8];
                    assembleTileLine(gb, palette, tile + 2 * row, dst);
                }
            }
        }
    }
}
