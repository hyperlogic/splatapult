#version 460

uniform sampler2D fontTex;
uniform vec4 color;

in vec2 frag_uv;

out vec4 out_color;

void main()
{
    vec4 texColor = texture2D(fontTex, frag_uv, -0.5f); // bias to increase sharpness a bit

    // premultiplied alpha blending
    out_color.rgb = color.a * color.rgb * texColor.rgb;
    out_color.a = color.a * texColor.a;
}
