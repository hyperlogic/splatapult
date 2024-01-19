/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

/*%%HEADER%%*/

uniform mat4 modelViewProjMat;

in vec3 position;
in vec2 uv;
in vec4 color;

out vec2 frag_uv;
out vec4 frag_color;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1.0f);
    frag_uv = uv;
    frag_color = color;
}
