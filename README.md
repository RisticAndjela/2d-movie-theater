# 2D Movie Theater — Kostur

Opis:
- Projekat: Bioskop (tema 7)
- Jezik: C++
- Grafika: OpenGL 3+ (programabilni pipeline)
- Windowing/Input: GLFW
- GL loader: GLAD
- Slike: stb_image
- Frame limiter: 75 FPS
- Fullscreen po startu, Escape za izlaz, kursorska slika

Build (preporučeno): CMake
Zavisnosti (instalirati pre build-a):
- GLFW
- GLAD (može se ukljuciti kao submodule ili preuzeti)
- glm (opciono)
- stb_image.h

Obavezni asseti (dodati u assets/):
- cursor_camera.png
- person.png
- (opciono) font.png ili FreeType set-up

Kratka uputstva:
1. Napravite folder `build`, pokrenite `cmake ..` i onda `cmake --build .`
2. Pokrenite binarnu datoteku. Aplikacija startuje u fullscreen-u; Escape zatvara.
3. Levim klikom se rezervišu/otkazuju sedišta.
4. Tasteri 1-9 kupuju N susednih mesta (implementacija traženja prvih N susednih iz desne strane poslednjeg reda u kodu - TODO).
5. Enter pokreće simulaciju ulaska/filma/izlaska (skeleton implementacije u main.cpp).
