/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// No lighting at all, solid color
//

/*%%HEADER%%*/

uniform mat4 modelViewProjMat;

in vec3 position;
in vec4 color;

out vec4 frag_color;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
	frag_color = color;
}
