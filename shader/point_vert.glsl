
//
// fullbright textured particle
//

uniform float pointSize;
uniform mat4 modelViewMat;
uniform mat4 projMat;
attribute vec3 position;
attribute vec2 uv;
attribute vec4 color;

varying vec4 frag_color;
varying vec2 frag_uv;

void main(void)
{
    vec4 viewPos = modelViewMat * vec4(position, 1);
    viewPos.xy += (uv - vec2(0.5f, 0.5f)) * pointSize;
    gl_Position = projMat * viewPos;
    frag_uv = uv;
    frag_color = color;
}
