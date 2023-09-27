//
// No lighting at all, solid color
//

uniform vec4 color;
uniform sampler2D colorTex;

varying vec2 frag_uv;

void main()
{
    vec4 texColor = texture2D(colorTex, frag_uv);

    // premultiplied alpha blending
    gl_FragColor.rgb = color.a * color.rgb * texColor.rgb;
    gl_FragColor.a = color.a * texColor.a;
}
