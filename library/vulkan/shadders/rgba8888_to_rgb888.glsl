#version 450
layout(local_size_x = 32) in;

layout(binding = 0) readonly buffer Input {
    uvec4 pixels[];
};

layout(binding = 1) writeonly buffer Output {
    uvec3 pixels[];
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uvec4 rgba = pixels[idx];
    pixels[idx] = uvec3(rgba.r, rgba.g, rgba.b);
}