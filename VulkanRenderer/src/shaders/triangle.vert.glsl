#version 450

const vec3 verts[] = {
  vec3(0, 0.5, 0),
  vec3(-0.5, -0.5, 0),
  vec3(0.5,-0.5,0)
};

void main()
{
  gl_Position = vec4(verts[gl_VertexIndex], 1.0);
}