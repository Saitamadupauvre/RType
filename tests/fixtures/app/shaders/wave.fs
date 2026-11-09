#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float time;
out vec4 finalColor;
void main() {
    vec2 uv = fragTexCoord + vec2(sin(fragTexCoord.y * 20.0 + time) * 0.01, 0.0);
    finalColor = texture(texture0, uv) * fragColor;
}
