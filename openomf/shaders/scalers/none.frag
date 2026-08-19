#version 300 es

precision highp float;
precision highp sampler2D;

// In
in vec2 tex_coord;
uniform vec2 texture_size;
uniform sampler2D framebuffer;

// Out
out vec4 color;

void main() {
    // This is a simple passthrough.
    color = texture(framebuffer, tex_coord);
}