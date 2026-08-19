#version 300 es

precision highp float;
precision highp int;
precision highp usampler2D;

out vec4 color;

in vec2 tex_coord;
flat in int transparency_index;
flat in int remap_offset;
flat in int remap_rounds;
flat in int palette_offset;
flat in int palette_limit;
flat in int opacity;
flat in uint options;

// Atlas texture (GL_R8UI or GL_R16UI): palette index per texel.
uniform usampler2D atlas;

// Remap texture (GL_R16UI, 1024x19): uint16 palette index (0-1023) per texel.
uniform usampler2D remaps;

const int MAGIC_REMAP_ROUNDS = 12;
const float PHI = 1.61803398874989484820459;
const float ATLAS_H = 2048.0;
const vec2 NATIVE_SIZE = vec2(320.0, 200.0);

float noise(in vec2 v) {
    return fract(tan(10.0 * PHI * v.y) + (float(0x6b) * v.x) / 256.0);
}

// Encode a palette index into the paletted framebuffer (GL_RGBA16).
vec4 handle(int index, bool sprite_dark_tint, bool sprite_index_add,
            int r_rounds, int r_offset) {
    if (sprite_dark_tint) {
        float remap = float(r_offset + MAGIC_REMAP_ROUNDS * 19) / 255.0;
        return vec4(0.0, remap, float(index) / 1023.0, 0.0);
    }
    if (r_rounds > 0) {
        float remap = float(r_offset + r_rounds * 19 + index) / 255.0;
        return vec4(0.0, remap, 0.0, 0.0);
    }
    if (sprite_index_add) {
        float add = float(index * 60) / 1023.0;
        return vec4(0.0, 0.0, 0.0, add);
    }
    return vec4(float(index) / 1023.0, 0.0, 0.0, 0.0);
}

void main() {
    bool sprite_remap = (options & 1u) != 0u;
    bool sprite_shadowmask = (options & 2u) != 0u;
    bool sprite_index_add = (options & 4u) != 0u;
    bool sprite_har_quirks = (options & 8u) != 0u;
    bool sprite_dark_tint = (options & 0x10u) != 0u;

    // Don't render if we're decimating due to opacity
    float decimate_limit = float(opacity) / 255.0;
    float decimate_value = noise(gl_FragCoord.xy);
    if (decimate_value > decimate_limit) {
        discard;
    }

    if (sprite_shadowmask) {
        // make four samples to generate coverage
        int coverage = 0;
        for(int y = 0; y < 4; y++) {
            float offset = float(y - 1) / ATLAS_H;
            uvec4 texel = texture(atlas, tex_coord + vec2(0.0, offset));
            int index = int(texel.r);
            coverage += int(index != transparency_index);
        }

        // don't draw transparent pixels
        if(coverage == 0) {
            discard;
        }

        color = handle(coverage, sprite_dark_tint, sprite_index_add, remap_rounds, remap_offset);
        return;
    }


    uvec4 texel = texture(atlas, tex_coord);

    // Don't render if it's transparent pixel
    int index = int(texel.r);
    if (index == transparency_index) {
        discard;
    }

    // Palette offset and limit (for e.g. fonts)
    if (index <= palette_limit) {
        index = clamp(index + palette_offset, 0, palette_limit);
    }

    bool no_remap = sprite_har_quirks && index > 0x30;

    // If remapping is on, do it now.
    if (sprite_remap && !no_remap) {
        uvec4 remap = texelFetch(remaps, ivec2(index, remap_offset), 0);
        index = int(remap.r);
    }

    color = handle(index, sprite_dark_tint, sprite_index_add, remap_rounds, remap_offset);
}