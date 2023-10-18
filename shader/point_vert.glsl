
//
// fullbright textured particle
//

#version 460

uniform float pointSize;
uniform mat4 modelViewMat;
uniform mat4 projMat;

in vec3 position;
in vec2 uv;
in vec4 color;

out vec4 frag_color;
out vec2 frag_uv;

void main(void)
{
    vec4 viewPos = modelViewMat * vec4(position, 1);
    viewPos.xy += (uv - vec2(0.5f, 0.5f)) * pointSize;
    gl_Position = projMat * viewPos;
    frag_uv = uv;
    frag_color = color;
}
