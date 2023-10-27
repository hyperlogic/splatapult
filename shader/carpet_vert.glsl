//
// fullbright textured mesh
//

#version 460

uniform mat4 modelViewProjMat;

in vec3 position;
in vec2 uv;

out vec2 frag_uv;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
    frag_uv = uv;
}
