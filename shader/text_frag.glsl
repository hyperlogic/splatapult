#version 460

uniform sampler2D fontTex;
uniform vec4 color;

in vec2 frag_uv;
in vec4 frag_color;

out vec4 out_color;

void main()
{
    vec4 texColor = texture2D(fontTex, frag_uv);

    // premultiplied alpha blending
    out_color.rgb = color.a * color.rgb * texColor.rgb;
    out_color.g = 1.0f;
    out_color.a = 1.0f;
    //out_color.a = color.a * texColor.a;
}
