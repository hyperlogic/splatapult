//
// fullbright textured mesh
//

/*%%HEADER%%*/

uniform sampler2D colorTex;

in vec2 frag_uv;

out vec4 out_color;

void main()
{
    vec4 texColor = texture(colorTex, frag_uv);

    // premultiplied alpha blending
    out_color.rgb = texColor.a * texColor.rgb;
    out_color.a = texColor.a;
}
