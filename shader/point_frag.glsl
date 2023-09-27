//
// fullbright textured particle
//

uniform sampler2D colorTex;

varying vec2 frag_uv;
varying vec4 frag_color;

void main()
{
    vec4 texColor = texture2D(colorTex, frag_uv);

    // premultiplied alpha blending
    gl_FragColor.rgb = frag_color.a * frag_color.rgb * texColor.rgb;
    gl_FragColor.a = frag_color.a * texColor.a;
}
