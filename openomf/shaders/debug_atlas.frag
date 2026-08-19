#version 300 es

precision highp float;
precision highp sampler2D;
precision highp usampler2D;

in vec2 tex_coord;
uniform sampler2D palette;
uniform usampler2D atlas;

out vec4 color;

void main() {
    uvec4 texel = texture(atlas, tex_coord);
    ivec2 index = ivec2(texel.r, 0);
    color = texelFetch(palette, index, 0);
}