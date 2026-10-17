/*
 * 🐆 Lince — letra.c
 * Envoltorio de stb_truetype (src/terceros) para el módulo juego.
 */
#include "letra.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "terceros/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int letra_preparar(const unsigned char *bytes, size_t largo, float tam,
                   const int *cps, int ncps, LetraAtlas *a) {
    memset(a, 0, sizeof *a);
    stbtt_fontinfo info;
    if (largo < 12 || stbtt_GetNumberOfFonts(bytes) < 1) return 0;
    int off = stbtt_GetFontOffsetForIndex(bytes, 0);
    if (off < 0 || !stbtt_InitFont(&info, bytes, off)) return 0;
    /* sólo las letras que la fuente tiene (las demás saldrían como una caja) */
    int *tiene = malloc(sizeof(int) * (size_t)ncps), n = 0;
    if (!tiene) return 0;
    for (int i = 0; i < ncps; i++) if (stbtt_FindGlyphIndex(&info, cps[i])) tiene[n++] = cps[i];
    if (n == 0) { free(tiene); return 0; }
    stbtt_packedchar *pc = malloc(sizeof(stbtt_packedchar) * (size_t)n);
    if (!pc) { free(tiene); return 0; }
    stbtt_pack_range r;
    memset(&r, 0, sizeof r);
    r.font_size = tam;
    r.array_of_unicode_codepoints = tiene;
    r.num_chars = n;
    r.chardata_for_range = pc;
    /* el atlas más pequeño en que caben */
    unsigned char *alfa = NULL;
    int lado = 128, ok = 0;
    for (; lado <= 8192 && !ok; lado *= 2) {
        free(alfa);
        alfa = calloc((size_t)lado * lado, 1);
        if (!alfa) break;
        stbtt_pack_context ctx;
        if (!stbtt_PackBegin(&ctx, alfa, lado, lado, 0, 1, NULL)) break;
        ok = stbtt_PackFontRanges(&ctx, bytes, 0, &r, 1);
        stbtt_PackEnd(&ctx);
        if (ok) break;
    }
    if (!ok) { free(alfa); free(pc); free(tiene); return 0; }
    a->ancho = a->alto = lado;
    a->rgba = malloc((size_t)lado * lado * 4);
    a->glifos = malloc(sizeof(LetraGlifo) * (size_t)n);
    if (!a->rgba || !a->glifos) { free(alfa); free(pc); free(tiene); letra_liberar(a); return 0; }
    for (size_t i = 0; i < (size_t)lado * lado; i++) {
        a->rgba[i * 4] = a->rgba[i * 4 + 1] = a->rgba[i * 4 + 2] = 255;
        a->rgba[i * 4 + 3] = alfa[i];
    }
    free(alfa);
    for (int i = 0; i < n; i++) {
        LetraGlifo *g = &a->glifos[i];
        g->cp = tiene[i];
        g->sx = pc[i].x0; g->sy = pc[i].y0;
        g->sw = pc[i].x1 - pc[i].x0; g->sh = pc[i].y1 - pc[i].y0;
        g->dx = pc[i].xoff; g->dy = pc[i].yoff;
        g->avance = pc[i].xadvance;
    }
    a->nglifos = n;
    free(pc);
    float escala = stbtt_ScaleForPixelHeight(&info, tam);
    int asc, desc, hueco;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &hueco);
    a->ascenso = asc * escala;
    a->alto_linea = (asc - desc + hueco) * escala;
    /* kerning de las letras normales: "AV" se junta un poco */
    int hay = 0;
    a->kerning = calloc(95 * 95, sizeof(float));
    for (int c1 = 32; a->kerning && c1 < 127; c1++)
        for (int c2 = 32; c2 < 127; c2++) {
            int k = stbtt_GetCodepointKernAdvance(&info, c1, c2);
            if (k) { a->kerning[(c1 - 32) * 95 + (c2 - 32)] = k * escala; hay = 1; }
        }
    if (!hay) { free(a->kerning); a->kerning = NULL; }
    free(tiene);
    return 1;
}

void letra_liberar(LetraAtlas *a) {
    free(a->glifos);
    free(a->rgba);
    free(a->kerning);
    memset(a, 0, sizeof *a);
}
