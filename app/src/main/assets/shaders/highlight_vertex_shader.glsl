#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform vec3 uOffset;

void main() {
    gl_Position = uMVP * vec4(aPos + uOffset, 1.0);
}
