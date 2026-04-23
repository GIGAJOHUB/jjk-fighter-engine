#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#pragma pack(push, 1)
typedef struct {
    uint16_t group, itemno, width, height;
    int16_t axisx, axisy;
    uint16_t linkedIndex;
    uint8_t fmt, coldepth;
    uint32_t dataOffset, dataLength;
    uint16_t palIndex, flags;
} SN;
#pragma pack(pop)
int main() {
    FILE* f = fopen("Sukuna Ryomen S1\\Sukuna.sff", "rb");
    fseek(f, 36, SEEK_SET);
    uint32_t so, sc, po, pc; 
    fread(&so,4,1,f); fread(&sc,4,1,f); fread(&po,4,1,f); fread(&pc,4,1,f);
    uint32_t ldo, ldl, tdo, tdl;
    fread(&ldo,4,1,f); fread(&ldl,4,1,f); fread(&tdo,4,1,f); fread(&tdl,4,1,f);
    printf("Sprite offset: 0x%X, count: %u\n", so, sc);
    SN* nodes = (SN*)calloc(sc, sizeof(SN));
    fseek(f, so, SEEK_SET);
    fread(nodes, sizeof(SN), sc, f);
    int fmtCounts[16] = {0};
    for (uint32_t i = 0; i < sc; i++) { if (nodes[i].fmt < 16) fmtCounts[nodes[i].fmt]++; }
    for (int i = 0; i < 16; i++) { if (fmtCounts[i]) printf("Format %d: %d sprites\n", i, fmtCounts[i]); }
    for (int i = 0; i < 15 && i < (int)sc; i++) {
        printf("  [%d] g=%u i=%u %ux%u fmt=%u depth=%u off=0x%X len=%u flags=%u\n",
            i, nodes[i].group, nodes[i].itemno, nodes[i].width, nodes[i].height,
            nodes[i].fmt, nodes[i].coldepth, nodes[i].dataOffset, nodes[i].dataLength, nodes[i].flags);
    }
    free(nodes);
    fclose(f);
    return 0;
}
