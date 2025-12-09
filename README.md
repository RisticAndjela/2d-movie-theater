```markdown
# 2D Movie Theater (Tema 7) - Implementacija

Opis:
- Jezik: C++
- Grafika: OpenGL 3.3+ (programabilni pipeline)
- Windowing/Input: GLFW
- Frame limiter: 75 FPS
- Fullscreen on start, Escape to exit
- Kursorska slika: proceduralno nacrtana filmska kamera
- Sedišta: najmanje 54 (6x9)
- Rezervisanje sedista levim klikom (plavo -> žuto)
- Kupovina pritiskom na tastere 1-9 (crveno)
- Enter: simulacija ulaska, projekcija (20s), izlazak, reset

Build:
1. Instalirajte zavisnosti: GLFW, GLAD (ili dodajte glad source), OpenGL
2. mkdir build && cd build
3. cmake ..
4. cmake --build .

Napomena: Kod očekuje GLAD da bude dostupan (možete ubaciti glad.c/h u projekt ili linkovati sistemsku biblioteku). Koristi stb_easy_font.h (uključen) za crtanje teksta.
```
