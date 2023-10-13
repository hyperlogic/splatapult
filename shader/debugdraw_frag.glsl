//
// No lighting at all, solid color
//

varying vec4 frag_color;

void main()
{
	// pre-multiplied alpha blending
    gl_FragColor.rgb = frag_color.a * frag_color.rgb;
	gl_FragColor.a = frag_color.a;
}
