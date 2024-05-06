precision mediump float;

uniform sampler2D texture;
uniform float pixel_threshold;
uniform mat4 sf_texture;

varying vec2 v_texCoord;
varying vec4 v_color;

void main()
{
    vec4 coord = sf_texture * vec4(v_texCoord, 0.0, 1.0);
    float factor = 1.0 / (pixel_threshold + 0.001);
    vec2 pos = floor(coord.xy * factor + 0.5) / factor;
    gl_FragColor = texture2D(texture, pos) * v_color;
}
