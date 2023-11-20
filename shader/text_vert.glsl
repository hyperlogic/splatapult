#version 460

uniform mat4 modelViewProjMat;
uniform vec4 color;

in vec2 position;
in vec2 uv;

out vec2 frag_uv;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 0.0f, 1.0f);
    frag_uv = uv;
}
