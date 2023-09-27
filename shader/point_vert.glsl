
//
// No lighting at all, solid color, single texture
//

uniform vec4 color;
uniform mat4 modelViewProjMat;
attribute vec3 position;
attribute vec2 uv;

varying vec2 frag_uv;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
    frag_uv = uv;
}
