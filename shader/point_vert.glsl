
//
// fullbright textured particle
//

#version 460

uniform float pointSize;
uniform float invAspectRatio;
uniform mat4 modelViewMat;
uniform mat4 projMat;

in vec3 position;
in vec4 color;

out vec4 geom_color;

void main(void)
{
    vec4 viewPos = modelViewMat * vec4(position, 1);
    gl_Position = projMat * viewPos;
    geom_color = color;
}
