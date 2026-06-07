#version 450
layout(local_size_x = 32) in;

layout(binding = 0) readonly buffer Input {
    uvec4 inPixels[];
};

layout(binding = 1) writeonly buffer Output {
    uvec3 outPixels[];
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uvec4 rgba = inPixels[idx];
    outPixels[idx] = uvec3(rgba.r, rgba.g, rgba.b);
}
