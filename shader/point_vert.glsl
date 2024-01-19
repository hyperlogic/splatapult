/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured particle
//

/*%%HEADER%%*/

uniform float pointSize;
uniform float invAspectRatio;
uniform mat4 modelViewMat;
uniform mat4 projMat;

in vec4 position;
in vec4 color;

out vec4 geom_color;

void main(void)
{
    gl_Position = projMat * modelViewMat * position;
    geom_color = color;
}
