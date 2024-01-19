/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

/*%%HEADER%%*/

uniform float pointSize;
uniform float invAspectRatio;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 geom_color[];

out vec4 frag_color;
out vec2 frag_uv;

void main()
{
	vec2 offset = vec2(pointSize * invAspectRatio, pointSize);

	// bottom-left vertex
    frag_uv = vec2(0.0, 0.0);
    frag_color = geom_color[0];
    gl_Position = gl_in[0].gl_Position + vec4(-offset.x, -offset.y, 0.0, 0.0);
    EmitVertex();

    // bottom-right vertex
    frag_uv = vec2(1.0, 0.0);
    frag_color = geom_color[0];
    gl_Position = gl_in[0].gl_Position + vec4(offset.x, -offset.y, 0.0, 0.0);
    EmitVertex();

    // top-left vertex
    frag_uv = vec2(0.0, 1.0);
    frag_color = geom_color[0];
    gl_Position = gl_in[0].gl_Position + vec4(-offset.x, offset.y, 0.0, 0.0);
    EmitVertex();

    // top-right vertex
    frag_uv = vec2(1.0, 1.0);
    frag_color = geom_color[0];
    gl_Position = gl_in[0].gl_Position + vec4(offset.x, offset.y, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
