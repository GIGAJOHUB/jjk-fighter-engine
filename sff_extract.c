/*
 * SFFv2 Full Extractor - handles RLE8, raw, and LZ5 formats
 * Extracts sprite frames as PNG using Raylib's image API
 */
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <direct.h>
#include <sys/stat.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t group, itemno, width, height;
    int16_t  axisx, axisy;
    uint16_t linkedIndex;
    uint8_t  fmt, coldepth;
    uint32_t dataOffset, dataLength;
    uint16_t palIndex, flags;
} SFFv2SpriteNode;

typedef struct {
    uint16_t group, itemno;
    uint16_t numcols;
    uint16_t linkedIndex;
    uint32_t dataOffset, dataLength;
} SFFv2PaletteNode;
#pragma pack(pop)

typedef struct {
    uint8_t r, g, b, a;
} RGBA;

static RGBA gPalettes[1024][256];  /* up to 1024 palettes, 256 colors each */
static int gPaletteCount = 0;

static void CreateDirIfNeeded(const char* dir) {
    struct stat st;
    if (stat(dir, &st) != 0) _mkdir(dir);
}

/* Decode RLE8 data into pixel indices */
static int DecodeRLE8(const uint8_t* src, uint32_t srcLen, uint8_t* dst, uint32_t dstLen) {
    uint32_t si = 0, di = 0;
    while (si < srcLen && di < dstLen) {
        uint8_t b = src[si++];
        if ((b & 0xC0) == 0x40) {
            /* RLE run: next byte repeated (b & 0x3F) times */
            int count = (b & 0x3F);
            if (count == 0) count = 1;
            uint8_t val = (si < srcLen) ? src[si++] : 0;
            for (int j = 0; j < count && di < dstLen; j++) dst[di++] = val;
        } else {
            dst[di++] = b;
        }
    }
    return (int)di;
}

/* Decode LZ5 data into pixel indices */
static int DecodeLZ5(const uint8_t* src, uint32_t srcLen, uint8_t* dst, uint32_t dstLen) {
    /* LZ5 is a simple LZ variant used in SFFv2 */
    uint32_t si = 0, di = 0;
    /* First 4 bytes = decompressed size */
    if (srcLen < 4) return 0;
    uint32_t decompSize = src[0] | (src[1]<<8) | (src[2]<<16) | (src[3]<<24);
    si = 4;
    if (decompSize > dstLen) decompSize = dstLen;
    
    uint8_t recycled = 0;
    int bitsLeft = 0;
    uint8_t controlByte = 0;
    int controlBits = 0;
    
    while (si < srcLen && di < decompSize) {
        if (controlBits == 0) {
            if (si >= srcLen) break;
            controlByte = src[si++];
            controlBits = 8;
        }
        controlBits--;
        if ((controlByte >> controlBits) & 1) {
            /* Literal byte */
            if (si >= srcLen) break;
            dst[di++] = src[si++];
            recycled = dst[di-1];
        } else {
            /* Copy from back-reference */
            if (si + 1 >= srcLen) break;
            uint8_t b1 = src[si++];
            uint8_t b2 = src[si++];
            
            int length, offset;
            if ((b1 & 0x3F) == 0) {
                /* Short RLE using recycled byte */
                length = b2;
                if (length == 0) break; /* end marker */
                for (int j = 0; j < length && di < decompSize; j++)
                    dst[di++] = recycled;
                continue;
            }
            
            offset = b1 >> 6;
            offset = (offset << 8) | b2;
            length = (b1 & 0x3F);
            
            if (offset == 0 || (int)di - offset < 0) {
                /* Invalid reference, write zeros */
                for (int j = 0; j < length && di < decompSize; j++)
                    dst[di++] = 0;
            } else {
                int srcPos = (int)di - offset;
                for (int j = 0; j < length && di < decompSize; j++) {
                    dst[di++] = dst[srcPos + (j % offset)];
                }
            }
        }
    }
    return (int)di;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: sff_extract2 <input.sff> <output_dir> [group_filter]\n");
        return 1;
    }

    const char* sffPath = argv[1];
    const char* outDir  = argv[2];
    int groupFilter = -1;
    int maxExtract = 500; /* safety limit */
    if (argc > 3) groupFilter = atoi(argv[3]);
    if (argc > 4) maxExtract = atoi(argv[4]);

    FILE* f = fopen(sffPath, "rb");
    if (!f) { printf("Cannot open: %s\n", sffPath); return 1; }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char sig[12];
    fread(sig, 1, 12, f);
    if (memcmp(sig, "ElecbyteSpr", 11) != 0) {
        printf("Not a valid SFF file\n"); fclose(f); return 1;
    }
    uint8_t ver[4];
    fread(ver, 1, 4, f);
    printf("SFF version: %d.%d.%d.%d\n", ver[3], ver[2], ver[1], ver[0]);
    if (ver[3] != 2) { printf("Only SFFv2 supported\n"); fclose(f); return 1; }

    fseek(f, 36, SEEK_SET);
    uint32_t spriteOffset, spriteCount, paletteOffset, paletteCount;
    uint32_t ldataOffset, ldataLength, tdataOffset, tdataLength;
    fread(&spriteOffset, 4, 1, f);
    fread(&spriteCount, 4, 1, f);
    fread(&paletteOffset, 4, 1, f);
    fread(&paletteCount, 4, 1, f);
    fread(&ldataOffset, 4, 1, f);
    fread(&ldataLength, 4, 1, f);
    fread(&tdataOffset, 4, 1, f);
    fread(&tdataLength, 4, 1, f);

    printf("Sprites: %u, Palettes: %u\n", spriteCount, paletteCount);
    printf("LData: 0x%X (%u bytes), TData: 0x%X (%u bytes)\n", ldataOffset, ldataLength, tdataOffset, tdataLength);

    /* Read palettes */
    if (paletteCount > 1024) paletteCount = 1024;
    SFFv2PaletteNode* palNodes = (SFFv2PaletteNode*)calloc(paletteCount, sizeof(SFFv2PaletteNode));
    fseek(f, paletteOffset, SEEK_SET);
    fread(palNodes, sizeof(SFFv2PaletteNode), paletteCount, f);
    
    for (uint32_t i = 0; i < paletteCount; i++) {
        SFFv2PaletteNode* pn = &palNodes[i];
        if (pn->numcols == 0) pn->numcols = 256;
        uint32_t palOff = ldataOffset + pn->dataOffset;
        if (palOff + pn->numcols * 4 > (uint32_t)fileSize) continue;
        
        fseek(f, palOff, SEEK_SET);
        uint8_t palData[256*4];
        int readCols = pn->numcols > 256 ? 256 : pn->numcols;
        fread(palData, 4, readCols, f);
        for (int c = 0; c < readCols; c++) {
            gPalettes[i][c].r = palData[c*4+0];
            gPalettes[i][c].g = palData[c*4+1];
            gPalettes[i][c].b = palData[c*4+2];
            gPalettes[i][c].a = (c == 0) ? 0 : 255;  /* color 0 = transparent */
        }
    }
    gPaletteCount = (int)paletteCount;
    printf("Loaded %d palettes\n", gPaletteCount);

    /* Read sprite nodes */
    SFFv2SpriteNode* nodes = (SFFv2SpriteNode*)calloc(spriteCount, sizeof(SFFv2SpriteNode));
    fseek(f, spriteOffset, SEEK_SET);
    fread(nodes, sizeof(SFFv2SpriteNode), spriteCount, f);

    CreateDirIfNeeded(outDir);

    int extracted = 0;
    for (uint32_t i = 0; i < spriteCount && extracted < maxExtract; i++) {
        SFFv2SpriteNode* n = &nodes[i];
        if (groupFilter >= 0 && n->group != (uint16_t)groupFilter) continue;
        if (n->width == 0 || n->height == 0) continue;
        if (n->fmt == 1 && n->dataLength == 0) continue; /* linked sprite, skip */

        uint32_t dataOff = (n->flags & 1) ? (tdataOffset + n->dataOffset) : (ldataOffset + n->dataOffset);
        if (n->dataLength == 0 || dataOff + n->dataLength > (uint32_t)fileSize) continue;

        /* Read compressed data */
        uint8_t* compData = (uint8_t*)malloc(n->dataLength);
        fseek(f, dataOff, SEEK_SET);
        fread(compData, 1, n->dataLength, f);

        uint32_t pixelCount = (uint32_t)n->width * n->height;
        uint8_t* indices = (uint8_t*)calloc(pixelCount, 1);
        int decoded = 0;

        if (n->fmt == 0 || n->fmt == 1) {
            /* Raw indexed */
            int copyLen = n->dataLength < pixelCount ? n->dataLength : pixelCount;
            memcpy(indices, compData, copyLen);
            decoded = copyLen;
        } else if (n->fmt == 2) {
            /* RLE8 */
            decoded = DecodeRLE8(compData, n->dataLength, indices, pixelCount);
        } else if (n->fmt == 4) {
            /* LZ5 */
            decoded = DecodeLZ5(compData, n->dataLength, indices, pixelCount);
        }

        if (decoded > 0) {
            /* Build RGBA image */
            int palIdx = n->palIndex;
            if (palIdx >= gPaletteCount) palIdx = 0;

            Image img = GenImageColor(n->width, n->height, BLANK);
            Color* pixels = LoadImageColors(img);
            for (uint32_t p = 0; p < pixelCount; p++) {
                RGBA c = gPalettes[palIdx][indices[p]];
                pixels[p] = (Color){c.r, c.g, c.b, c.a};
            }
            
            /* Replace pixels in image */
            for (int y = 0; y < n->height; y++) {
                for (int x = 0; x < n->width; x++) {
                    ImageDrawPixel(&img, x, y, pixels[y * n->width + x]);
                }
            }

            char outPath[512];
            snprintf(outPath, sizeof(outPath), "%s/g%04u_i%04u.png", outDir, n->group, n->itemno);
            ExportImage(img, outPath);
            UnloadImageColors(pixels);
            UnloadImage(img);
            extracted++;
            if (extracted % 100 == 0) printf("  ... extracted %d sprites ...\n", extracted);
        }

        free(indices);
        free(compData);
    }

    printf("\nDone! Extracted %d sprites to %s\n", extracted, outDir);
    free(nodes);
    free(palNodes);
    fclose(f);
    return 0;
}
