#version 430 core

layout (location = 0) in vec4 vertPos;
uniform mat4 MVP;
out float color;

void main()
{
  color = vertPos.w;
  gl_Position =  MVP * vec4(vertPos.xyz, 1.0);
}
