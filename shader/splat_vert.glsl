
//
// WIP splat vertex shader
//

uniform mat4 projMat;

attribute vec3 viewPos;

varying vec4 frag_color;

void main(void)
{
    gl_Position = projMat * vec4(viewPos, 1);
    frag_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
