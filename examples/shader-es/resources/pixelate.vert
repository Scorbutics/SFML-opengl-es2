attribute vec4 position;
attribute vec4 color;
attribute vec4 texCoord;

varying vec4 v_texCoord;
varying vec4 v_color;
varying vec4 v_position;

void main() {
    gl_Position = position;
    v_texCoord = texCoord;
    v_color = color;
    v_position = position;
}
