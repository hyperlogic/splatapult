//
// No lighting at all, solid color
//

#version 120

uniform mat4 modelViewProjMat;

attribute vec3 position;
attribute vec4 color;

varying vec4 frag_color;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
	frag_color = color;
}
