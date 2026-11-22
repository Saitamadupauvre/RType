#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float time;
uniform vec2 resolution;

out vec4 finalColor;

const float HORIZON = 560.0 / 1080.0;

float hash(vec2 point) {
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    float t = clamp((uv.y - HORIZON) / (1.0 - HORIZON), 0.0, 1.0);
    float depth = pow(t, 1.7);

    vec3 near_color = vec3(0.78, 0.90, 0.92);
    vec3 far_color = vec3(0.03, 0.14, 0.20);
    vec3 color = mix(near_color, far_color, pow(t, 0.7));

    float rows = mix(90.0, 10.0, depth);
    float scroll = time * mix(0.3, 1.3, depth);
    float ripple_x = sin(uv.x * 7.0 + depth * 3.0 + scroll * 0.3) * 0.06;
    float wave = sin((t + ripple_x) * rows - scroll) * 0.5 + 0.5;
    float row_contrast = mix(0.05, 0.22, 1.0 - depth);
    color -= wave * row_contrast;

    float sparkle_grid = mix(300.0, 50.0, depth);
    vec2 cell = floor(vec2(uv.x * sparkle_grid, t * sparkle_grid * 0.35 + scroll * 0.6));
    float sparkle_chance = hash(cell);
    float sparkle = step(0.985, sparkle_chance) * (0.5 + 0.5 * sin(time * 6.0 + sparkle_chance * 80.0));
    color += sparkle * (1.0 - depth) * 0.5;

    float horizon_glow = smoothstep(0.08, 0.0, t) * 0.5;
    color += horizon_glow * vec3(0.5, 0.65, 0.68);

    finalColor = vec4(color, 1.0);
}
