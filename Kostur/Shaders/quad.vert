#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aTex;
out vec2 vTex;
uniform mat4 uProj;
uniform mat4 uModel;
void main(){ vTex = aTex; gl_Position = uProj * uModel * vec4(aPos, 0.0, 1.0); }
