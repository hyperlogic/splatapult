#version 460

uniform vec4 color;
uniform sampler2D colorTexture;

in vec2 frag_uv;

out vec4 out_color;

void main(void)
{
    //vec4 texColor = texture2D(colorTexture, frag_uv);

    // per pixel screen space partial derivatives
    vec2 dx = dFdx(frag_uv) * 0.25; // horizontal offset
    vec2 dy = dFdy(frag_uv) * 0.25; // vertical offset

    // supersampled 2x2 ordered grid
    vec4 texColor = vec4(0);
    texColor += texture2D(colorTexture, vec2(frag_uv + dx + dy));
    texColor += texture2D(colorTexture, vec2(frag_uv - dx + dy));
    texColor += texture2D(colorTexture, vec2(frag_uv + dx - dy));
    texColor += texture2D(colorTexture, vec2(frag_uv - dx - dy));
    texColor *= 0.25;

    // premultiplied alpha blending
    out_color.rgb = color.a * color.rgb * texColor.rgb;
    out_color.a = color.a * texColor.a;
}
