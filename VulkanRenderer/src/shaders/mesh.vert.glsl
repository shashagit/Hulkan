#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

struct Vertex
{
    float vx, vy, vz;
    uint8_t nx, ny, nz;
    float tu, tv;
};

layout(binding = 0) buffer Vertices
{
    Vertex vertices[];
};

layout( push_constant) uniform constants
{
    vec4 data;
    mat4 transformationMatrix;
} PushConstants;

layout(location = 0) out vec2 fragTexCoord;

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    vec3 position = vec3(v.vx, v.vy, v.vz);
    vec3 normal = vec3(v.nx, v.ny, v.nz) / 127.0 - 1.0;
    vec2 texCoord = vec2(v.tu, v.tv);

    //gl_Position = vec4(position + vec3(0, 0, 0.5), 1);

    gl_Position = PushConstants.transformationMatrix * vec4(position, 1.0);
    
    //color = vec4(normal * 0.5 + 0.3, 1.0);
    fragTexCoord = texCoord;
}