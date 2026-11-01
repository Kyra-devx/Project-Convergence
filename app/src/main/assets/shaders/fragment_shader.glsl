#version 300 es
precision mediump float;

in vec3 vColor;
in vec3 vNormal;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.3);
    vec3 finalColor = vColor * diffuse;
    FragColor = vec4(finalColor, 1.0);
}
