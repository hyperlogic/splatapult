/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured particle
//

/*%%HEADER%%*/

uniform sampler2D colorTex;

in vec2 frag_uv;
in vec4 frag_color;

out vec4 out_color;

void main()
{
    vec4 texColor = texture(colorTex, frag_uv);

    // premultiplied alpha blending
    out_color.rgb = frag_color.a * frag_color.rgb * texColor.rgb;
    out_color.a = frag_color.a * texColor.a;
}
