#version 130
attribute vec3 position;
attribute vec4 color;
uniform mat4 Projection;
uniform mat4 View;
varying vec4 vertexColor;

void main(void)
{
    vertexColor = color;
    gl_Position = Projection * View * vec4(position, 1.0);
}
