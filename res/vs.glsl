attribute vec4 pos;
attribute vec2 aCoord;

varying vec2 coord;

uniform mat4 transform;
uniform mat4 projection;

void main()
{
    coord = aCoord;
    gl_Position = projection * transform * pos;
}