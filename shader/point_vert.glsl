
//
// fullbright textured particle
//

uniform float size;
uniform mat4 modelViewMat;
uniform mat4 projMat;
attribute vec3 position;
attribute vec2 uv;

varying vec2 frag_uv;

void main(void)
{
    vec4 viewPos = modelViewMat * vec4(position, 1);
    viewPos.xy += (uv - vec2(0.5f, 0.5f)) * size;
    gl_Position = projMat * viewPos;
    frag_uv = uv;
}
