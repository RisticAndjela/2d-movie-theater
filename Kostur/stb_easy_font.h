#ifndef STB_EASY_FONT_H
#define STB_EASY_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

	// Deklaracije - implementacija treba biti SAMO u jednoj .cpp jedinici,
	// npr. Kostur/stb_easy_font.cpp koji ima #define STB_EASY_FONT_IMPLEMENTATION
	int stb_easy_font_print(float ox, float oy, const char* text, unsigned char* color, void* vertex_buffer, int vbuf_size);
	int stb_easy_font_width(const char* text);

#ifdef __cplusplus
}
#endif

#endif // STB_EASY_FONT_H