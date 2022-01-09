#include "gameboy.h"

#include <stdio.h>

uint8 getGrayLevel(uint8 color) {
    uint8 shades[4] = {255, 170, 85, 0};

    return shades[color];
}

uint8 getPaletteColor(uint8 palette, uint8 id) {
    uint8 color = (palette >> (id * 2)) & 0x3;

    return color;
}

void fetchSpritePixelRow(GameBoy* gb, uint8 ly, uint8 lx) {
    // TODO(octave) : 10 sprites per scan line limitation
    uint8 lyInSprite = ly + 16;
    uint8 lxInSprite = lx + 8;

    uint8 lcdc = IO(LCDC);
    uint8 spriteHeight = getBit(lcdc, 2) ? 16 : 8;

    uint8 wantedEnd = gb->spriteFifo.end + 8;
    
    for (uint32 spriteIndex = 0; spriteIndex < 40; spriteIndex++) {
        uint16 spriteAddr = SPRITE_ATTRIBUTE_TABLE + spriteIndex * 4;
        
        uint8 spriteY =   readMemory(gb, spriteAddr);
        uint8 spriteX =   readMemory(gb, spriteAddr + 1);
        uint8 tileIndex = readMemory(gb, spriteAddr + 2);
        uint8 flags =     readMemory(gb, spriteAddr + 3);

        uint16 tileAddr = SPRITE_TILES_TABLE + tileIndex * 16;

        uint8 palette = getBit(flags, 4) ?
            readMemory(gb, IO_OBP1) :
            readMemory(gb, IO_OBP0);

        if (lyInSprite >= spriteY && lyInSprite < spriteY + spriteHeight
            && lxInSprite >= spriteX && lxInSprite < spriteX + 8) {
            
            // TODO(octave) : factor this into function ?
            uint8 tileDataLow = readMemory(gb, tileAddr + (lyInSprite - spriteY) * 2);
            uint8 tileDataHigh = readMemory(gb, tileAddr + (lyInSprite - spriteY) * 2 + 1);

            // push up to 8 pixels to sprite fifo
            uint8 dx = lxInSprite - spriteX;
            for (uint8 pixel = 7; pixel < 8; pixel--) {
                uint8 pixelColorIndex =
                    (getBit(tileDataHigh, pixel) << 1) | getBit(tileDataLow, pixel);

                if (dx > 0) {
                    dx--;
                } else {
                    gb->spriteFifo.pixels[gb->spriteFifo.end++] =
                        (FIFOPixel){pixelColorIndex};
                }
            }

            break;
        }
    }

    /* while (gb->spriteFifo.end < wantedEnd) { */
        /* gb->spriteFifo.pixels[gb->spriteFifo.end++] = */
            /* (FIFOPixel){0}; */
    /* } */
}

void fetchBackgroundPixelRow(GameBoy* gb, uint8 ly, uint8 lx) {
    uint8 lcdc = IO(LCDC);
    uint8 wx = IO(WX);
    uint8 wy = IO(WY);

    bool32 inWindow =
        getBit(lcdc, 5)
        && lx >= wx
        && ly >= wy;

    inWindow = false;

    // find tilemap
    uint16 tilemapAddr;
    if (inWindow) {
        tilemapAddr = getBit(lcdc, 6) ? TILEMAP1 : TILEMAP0;
    } else {
        tilemapAddr = getBit(lcdc, 3) ? TILEMAP1 : TILEMAP0;
    }

    // compute tile row coordinates
    uint8 tileX, rowY;
    if (inWindow) {
        // TODO(octave) : offset x by 7 ?
        tileX = (lx - wx) / 8;
        rowY = (ly - wy);
    } else {
        uint8 scx = IO(SCX);
        uint8 scy = IO(SCY);
        
        tileX = (scx / 8 + lx / 8) & 0x1F;
        rowY = (ly + scy) & 0xFF;
    }

    // compute tile address
    uint8 tileIndex = readMemory(gb, tilemapAddr + (rowY / 8) * 32 + tileX);

    uint16 tileAddr;
    if (getBit(lcdc, 4)) {
        tileAddr = TILE_DATA_BLOCK0 + tileIndex * 16;
    } else {
        if (tileIndex < 128) {
            tileAddr = TILE_DATA_BLOCK2 + tileIndex * 16;
        } else {
            tileAddr = TILE_DATA_BLOCK1 + (tileIndex - 128) * 16;
        }
    }

    uint8 tileDataLow = readMemory(gb, tileAddr + (rowY % 8) * 2);
    uint8 tileDataHigh = readMemory(gb, tileAddr + (rowY % 8) * 2 + 1);

    // push 8 pixels to background fifo
    ASSERT((gb->backgroundFifo.end == 0 && gb->backgroundFifo.start == 0)
           || gb->backgroundFifo.end == 8);
    for (uint8 pixel = 7; pixel < 8; pixel--) {
        uint8 pixelColorIndex =
            (getBit(tileDataHigh, pixel) << 1) | getBit(tileDataLow, pixel);
        
        gb->backgroundFifo.pixels[gb->backgroundFifo.end++] =
            (FIFOPixel){pixelColorIndex};
    }
}

uint8 popBackgroundPixelColor(GameBoy* gb) {
    uint8 palette = IO(BGP);

    ASSERT(gb->backgroundFifo.start < 8 && gb->backgroundFifo.start < gb->backgroundFifo.end);

    FIFOPixel backgroundPixel = gb->backgroundFifo.pixels[gb->backgroundFifo.start++];

    return getPaletteColor(palette, backgroundPixel.colorIndex);
}

uint8 getNextBackgroundPixel(GameBoy* gb, uint8 y, uint8 x) {
    uint8 lcdc = IO(LCDC);

    ASSERT(gb->backgroundFifo.start <= gb->backgroundFifo.end);
    
    // Fill up FIFOs if necessary
    if (gb->backgroundFifo.start >= 8) {
        ASSERT(gb->backgroundFifo.start == 8);
        ASSERT(gb->backgroundFifo.end == 16);

        // shift fifo 8 spots to the left
        for (uint8 i = 0; i < 8; i++) {
            gb->backgroundFifo.pixels[i] = gb->backgroundFifo.pixels[i+8];
        }
        gb->backgroundFifo.end -= 8;
        gb->backgroundFifo.start -= 8;

        fetchBackgroundPixelRow(gb, y, x + 8);
    }

    uint8 popped = popBackgroundPixelColor(gb);
    
    if (getBit(lcdc, 0)) {
        return popped;
    } else {
        return 0;
    }

}

#if 0

void drawScreen(GameBoy* gb) {
    for (uint8 y = 0; y < GAMEBOY_SCREEN_HEIGHT; y++) {
        // clear FIFOs
        gb->backgroundFifo.start = 0;
        gb->backgroundFifo.end = 0;
        
        gb->spriteFifo.start = 0;
        gb->spriteFifo.end = 0;
        
        // fetch first 2 tiles
        fetchBackgroundPixelRow(gb, y, 0);
        fetchBackgroundPixelRow(gb, y, 8);
        
        // fetch first 2 tiles
        fetchSpritePixelRow(gb, y, 0);
        fetchSpritePixelRow(gb, y, 8);
        
        // skip the first few pixels if scrolled
        for (uint8 i = 0; i < IO(SCX) % 8; i++) {
            getNextBackgroundPixel(gb, y, 0);
        }

        for (uint8 x = 0; x < GAMEBOY_SCREEN_WIDTH; x++) {
            uint8 colorIndex = getNextBackgroundPixel(gb, y, x);
            uint8 gray = getGrayLevel(colorIndex);

            gb->screen[y][x] = gray;
        }
    }
}

#endif

uint8 getSpriteColor(GameBoy* gb, uint8 ly, uint8 lx) {
    uint8 lyInSprite = ly + 16;
    uint8 lxInSprite = lx + 8;

    uint8 lcdc = IO(LCDC);
    uint8 spriteHeight = getBit(lcdc, 2) ? 16 : 8;

    uint8 colorIndex = 0;
    uint8 minSpriteX = 255;
    uint8 minSpriteIndex = 255;

    for (uint8 spriteIndex = 0; spriteIndex < 40; spriteIndex++) {
        uint16 spriteAddr = SPRITE_ATTRIBUTE_TABLE + spriteIndex * 4;
        
        uint8 spriteY =   readMemory(gb, spriteAddr);
        uint8 spriteX =   readMemory(gb, spriteAddr + 1);
        uint8 tileIndex = readMemory(gb, spriteAddr + 2);
        uint8 flags =     readMemory(gb, spriteAddr + 3);

        uint16 tileAddr = SPRITE_TILES_TABLE + tileIndex * 16;

        uint8 palette = getBit(flags, 4) ?
            readMemory(gb, IO_OBP1) :
            readMemory(gb, IO_OBP0);

        uint8 dx = lxInSprite - spriteX;
        uint8 dy = lyInSprite - spriteY;

        if (getBit(flags, 6)) {
            dy = 7 - dy;
        }
            
        if (getBit(flags, 5)) {
            dx = 7 - dx;
        }
            
        if (lyInSprite >= spriteY && lyInSprite < spriteY + spriteHeight
            && lxInSprite >= spriteX && lxInSprite < spriteX + 8) {
            // TODO(octave) : factor this into function ?
            
            uint8 tileDataLow = readMemory(gb, tileAddr + dy * 2);
            uint8 tileDataHigh = readMemory(gb, tileAddr + dy * 2 + 1);

            uint8 pixelColorIndex =
                (getBit(tileDataHigh, 7 - dx) << 1) | getBit(tileDataLow, 7 - dx);

            if (pixelColorIndex &&
                (spriteX < minSpriteX ||
                 (spriteX == minSpriteX && spriteIndex < minSpriteIndex))) {
                colorIndex = pixelColorIndex;
                minSpriteX = spriteX;
                minSpriteIndex = spriteIndex;
            }
        }
    }

    return colorIndex;
}

void drawScreen(GameBoy* gb) {
    for (uint8 y = 0; y < GAMEBOY_SCREEN_HEIGHT; y++) {
        // clear FIFOs
        gb->backgroundFifo.start = 0;
        gb->backgroundFifo.end = 0;
        
        // fetch first 2 tiles
        fetchBackgroundPixelRow(gb, y, 0);
        fetchBackgroundPixelRow(gb, y, 8);
        
        // skip the first few pixels if scrolled
        for (uint8 i = 0; i < IO(SCX) % 8; i++) {
            getNextBackgroundPixel(gb, y, 0);
        }

        for (uint8 x = 0; x < GAMEBOY_SCREEN_WIDTH; x++) {
            uint8 sprite = getSpriteColor(gb, y, x);
            uint8 bg = getNextBackgroundPixel(gb, y, x);
            
            uint8 gray = getGrayLevel(sprite ? sprite : bg);

            gb->screen[y][x] = gray;
        }
    }
}
