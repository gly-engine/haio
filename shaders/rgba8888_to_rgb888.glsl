#version 450
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

layout(local_size_x = 32) in;

layout(binding = 0) readonly buffer Input {
    uint8_t inBytes[];
};

layout(binding = 1) writeonly buffer Output {
    uint8_t outBytes[];
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    outBytes[idx * 3 + 0] = inBytes[idx * 4 + 0];
    outBytes[idx * 3 + 1] = inBytes[idx * 4 + 1];
    outBytes[idx * 3 + 2] = inBytes[idx * 4 + 2];
}
