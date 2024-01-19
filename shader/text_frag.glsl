/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

/*%%HEADER%%*/

uniform sampler2D fontTex;

in vec2 frag_uv;
in vec4 frag_color;

out vec4 out_color;

void main()
{
    vec4 texColor = texture(fontTex, frag_uv, -0.5f); // bias to increase sharpness a bit

    // premultiplied alpha blending
    out_color.rgb = frag_color.a * frag_color.rgb * texColor.rgb;
    out_color.a = frag_color.a * texColor.a;
}
