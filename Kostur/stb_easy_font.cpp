#define STB_EASY_FONT_IMPLEMENTATION
#include "./stb_easy_font.h"
#include <cstring>

// Jednostavna implementacija koja generiše dve trouglaste liste po karakteru.
// vertex_buffer očekuje niz float-ova: x,y parovi (tri * num_tri * 2 floats).
// Vraća broj trouglova (triangles).
int stb_easy_font_print(float ox, float oy, const char* text, unsigned char* color, void* vertex_buffer, int vbuf_size)
{
    if (!vertex_buffer) return 0;
    float* v = (float*)vertex_buffer;
    int num_tri = 0;
    float x = ox;
    // sigurnosna provera: koliko floats može stati u buffer
    int max_floats = vbuf_size / (int)sizeof(float);
    int used_floats = 0;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        ++text;
        if (c < 32) { x += 8.0f; continue; } // skip control chars, advance pen
        // kvadrat karaktera width ~7, height ~8
        float x0 = x;
        float y0 = oy;
        float x1 = x0 + 7.0f;
        float y1 = oy + 8.0f;
        // treba upisati 6 vrhova * 2 komponente = 12 floats po karakteru (za 2 trougla)
        if (used_floats + 12 > max_floats) break;
        // triangle 1
        *v++ = x0; *v++ = y0;
        *v++ = x1; *v++ = y0;
        *v++ = x1; *v++ = y1;
        // triangle 2
        *v++ = x0; *v++ = y0;
        *v++ = x1; *v++ = y1;
        *v++ = x0; *v++ = y1;
        used_floats += 12;
        num_tri += 2;
        x += 8.0f; // advance pen
    }
    return num_tri;
}

int stb_easy_font_width(const char* text)
{
    if (!text) return 0;
    int count = 0;
    while (*text) { ++count; ++text; }
    return count * 8; // approx 8 pixels per char
}