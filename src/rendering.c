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

void fetchBackgroundPixelRow(GameBoy* gb, uint8 ly, uint8 lx) {
    uint8 lcdc = MMR_REG(LCDC);
    uint8 wx = MMR_REG(WX);
    uint8 wy = MMR_REG(WY);
    /* uint8 ly = MMR_REG(LY); */

    bool32 inWindow =
        lx >= wx
        && ly >= wy;

    inWindow = false;

    // find tilemap
    uint16 tilemapAddr;
    if (inWindow) {
        tilemapAddr = getBit(lcdc, 6) ? TILEMAP1 : TILEMAP0;
    } else {
        tilemapAddr = getBit(lcdc, 3) ? TILEMAP1 : TILEMAP0;
    }

    /* printf("tilemapAddr = %04X\n", tilemapAddr); */

    // compute tile row coordinates
    uint8 tileX, rowY;
    if (inWindow) {
        // TODO(octave) : offset x by 7 ?
        tileX = (lx - wx) / 8;
        rowY = (ly - wy);
    } else {
        uint8 scx = MMR_REG(SCX);
        uint8 scy = MMR_REG(SCY);
        
        tileX = (scx / 8 + lx / 8) & 0x1F;
        rowY = (ly + scy) & 0xFF;
    }

    /* printf("tileX = %02X, rowY = %02X, offset = %02X\n", tileX, rowY, (rowY / 8) * 32 + tileX); */
    
    // compute tile address
    uint8 tileIndex = readMemory(gb, tilemapAddr + (rowY / 8) * 32 + tileX);

    /* printf("tileIndex = %02X\n", tileIndex); */
    
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

    // TODO(octave) : check vertical flip
    uint8 tileDataLow = readMemory(gb, tileAddr + (rowY % 8) * 2);
    uint8 tileDataHigh = readMemory(gb, tileAddr + (rowY % 8) * 2 + 1);

    /* printf("Fetched %02X %02X\n", tileDataHigh, tileDataLow); */

    // push 8 pixels to background fifo
    // TODO(octave) : check horizontal flip
    ASSERT((gb->backgroundFifoEnd == 0 && gb->backgroundFifoStart == 0)
           || gb->backgroundFifoEnd == 8);
    for (uint8 pixel = 7; pixel < 8; pixel--) {
        uint8 pixelColorIndex =
            (getBit(tileDataHigh, pixel) << 1) | getBit(tileDataLow, pixel);
        
        gb->backgroundFifo[gb->backgroundFifoEnd++] =
            (FIFOPixel){pixelColorIndex};
    }
}

uint8 popPixelColor(GameBoy* gb) {
    uint8 palette = gb->memory[MMR_BGP];

    ASSERT(gb->backgroundFifoStart < 8);

    FIFOPixel backgroundPixel = gb->backgroundFifo[gb->backgroundFifoStart];
    gb->backgroundFifoStart++;

    // TODO(octave) : mix with sprite pixel

    return getPaletteColor(palette, backgroundPixel.colorIndex);
}

uint8 getNextPixel(GameBoy* gb, uint8 y, uint8 x) {
    ASSERT(gb->backgroundFifoStart <= gb->backgroundFifoEnd);
    
    // Fill up FIFO if necessary
    if (gb->backgroundFifoStart >= 8) {
        ASSERT(gb->backgroundFifoStart == 8);
        ASSERT(gb->backgroundFifoEnd == 16);

        // shift fifo 8 spots to the left
        for (uint8 i = 0; i < 8; i++) {
            gb->backgroundFifo[i] = gb->backgroundFifo[i+8];
        }
        gb->backgroundFifoEnd -= 8;
        gb->backgroundFifoStart -= 8;

        fetchBackgroundPixelRow(gb, y, x + 8);
    }

    return popPixelColor(gb);
}

void drawScreen(GameBoy* gb) {
    for (uint8 y = 0; y < GAMEBOY_SCREEN_HEIGHT; y++) {
        // clear fifo
        gb->backgroundFifoStart = 0;
        gb->backgroundFifoEnd = 0;
        
        // fetch first 2 tiles
        fetchBackgroundPixelRow(gb, y, 0);
        fetchBackgroundPixelRow(gb, y, 8);
        
        // skip the first few pixels if scrolled
        for (uint8 i = 0; i < MMR_REG(SCX) % 8; i++) {
            getNextPixel(gb, y, 0);
        }

        for (uint8 x = 0; x < GAMEBOY_SCREEN_WIDTH; x++) {
            uint8 colorIndex = getNextPixel(gb, y, x);
            uint8 gray = getGrayLevel(colorIndex);

            gb->screen[y][x] = gray;
        }
    }
}

