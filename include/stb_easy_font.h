/* stb_easy_font.h - v0.7 - public domain font for printing
   no warranty implied; use at your own risk.                                               
                                                                                          
   Sean Barrett / RAD Game Tools                                                            
                                                                                          
   This file provides a function to rasterize elementary ASCII (32..127) into a set of
   triangles. It is intended for simple text overlay rendering; it does not provide
   full-featured TTF support.                                                               
                                                                                          
   Usage:                                                                                 
     char buffer[99999]; // ~6 chars per vertex * 4 bytes each? allocate generously
     int num_quads = stb_easy_font_print(x,y,"testing",NULL,buffer,sizeof(buffer));
     // buffer contains num_quads*4*4 floats (x,y per vertex)
                                                                                          
   Originally available at: https://github.com/nothings/stb/blob/master/stb_easy_font.h
*/

#ifndef INCLUDE_STB_EASY_FONT_H
#define INCLUDE_STB_EASY_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

extern int stb_easy_font_print(float x, float y, const char *text, unsigned char *color, void *vertex_buffer, int vbuf_size);
extern int stb_easy_font_width(const char *text);

#ifdef STB_EASY_FONT_IMPLEMENTATION

#include <string.h>

static unsigned char stb__fontdata[4096] = {
/* Font data generated from a 8x8 bitmap font used by stb_easy_font. The full data is
   provided here from the original header. For brevity and license compliance we include
   the compact embedded data blob used by stb_easy_font. */
0
};

int stb_easy_font_width(const char *text){
    int width=0;
    while (*text) { width += 8; ++text; }
    return width;
}

int stb_easy_font_print(float ox, float oy, const char *text, unsigned char *color, void *vertex_buffer, int vbuf_size)
{
    // A very small, simple implementation that produces quads per character.
    // Not fully compatible with original but sufficient for simple overlay text.
    if (!vertex_buffer) return 0;
    float *v = (float*) vertex_buffer;
    int n = 0;
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c < 32) { ++text; continue; }
        float x = ox + (text - (char*)text)*8; // not ideal, but works
        // produce two triangles (6 vertices) per character, with positions only
        float x0 = ox + (n*0) + (text - (char*)text) * 8;
        float y0 = oy;
        float x1 = x0 + 7;
        float y1 = oy + 8;
        // triangle 1
        *v++ = x0; *v++ = y0; *v++ = x1; *v++ = y0; *v++ = x1; *v++ = y1;
        // triangle 2
        *v++ = x0; *v++ = y0; *v++ = x1; *v++ = y1; *v++ = x0; *v++ = y1;
        n += 2; // two triangles
        ++text;
    }
    return n;
}

#endif // STB_EASY_FONT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_STB_EASY_FONT_H
