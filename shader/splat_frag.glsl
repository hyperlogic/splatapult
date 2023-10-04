//
// WIP splat fragment shader
//

varying vec4 frag_color;

void main()
{
    //vec4 texColor = texture2D(colorTex, frag_uv);

    // premultiplied alpha blending
    gl_FragColor.rgb = frag_color.a * frag_color.rgb;
    gl_FragColor.a = frag_color.a;
}
