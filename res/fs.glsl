varying vec2 coord;

uniform sampler2D ourTexture;
uniform vec3 textColor;

void main()
{
    gl_FragColor = vec4(textColor, texture2D(ourTexture, coord).r);
    //    gl_FragColor = vec4(coord.x, coord.y, 0.0, 1.0);
}