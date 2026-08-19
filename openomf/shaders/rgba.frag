#version 300 es

precision highp float;
precision highp int;
precision highp sampler2D;
precision highp usampler2D;

// In
in vec2 tex_coord;
uniform sampler2D palette;

uniform sampler2D framebuffer;
uniform usampler2D remaps;

uniform uint framebuffer_options;

const int MAGIC_REMAP_ROUNDS = 12;

// Out
out vec4 color;

void set_color(int index) {
    ivec2 pal_index = ivec2(index, 0);
    color = texelFetch(palette, pal_index, 0);
}

void main() {
    bool fbufopt_credits = (framebuffer_options & 0x01u) != 0u;

    vec4 texel = textureLod(framebuffer, tex_coord, 0.0);
    int idx = int(texel.r * 1023.0 + 0.5);
    int remap_enc = int(texel.g * 255.0);
    int darktint = int(texel.b * 1023.0 + 0.5);
    int idx_add = int(texel.a * 1023.0 + 0.5);

    if(fbufopt_credits) {
        set_color(idx + idx_add);
        return;
    }

    int remap_row = remap_enc % 19;
    int remap_rounds = remap_enc / 19;

    idx += idx_add;

    if(darktint > 0 && remap_rounds != MAGIC_REMAP_ROUNDS){
        idx = darktint;
    } else if(darktint >= 0x60) {
        idx = darktint;
        remap_rounds = 0;
    } else if(darktint > 0) {
        uvec4 remap_val = texelFetch(remaps, ivec2(idx, 4), 0);
        int behind = 1 + clamp(int(remap_val.r) - 0xA8, 0, 7) * 2;
        idx = (darktint & 0xF0) + ((darktint & 0x0F) * 3 + behind * 2) / 5;
    }

    for (int i = 0; i < remap_rounds; i++) {
        uvec4 remap_texel = texelFetch(remaps, ivec2(idx, remap_row), 0);
        idx = int(remap_texel.r);
    }

    set_color(idx);
}