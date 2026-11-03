#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float time;
uniform vec2 resolution;

out vec4 finalColor;

float hash(vec2 point) {
    point = fract(point * vec2(123.34, 456.21));
    point += dot(point, point + 45.32);
    return fract(point.x * point.y);
}

float noise(vec2 point) {
    vec2 cell = floor(point);
    vec2 local = fract(point);
    vec2 blend = local * local * (3.0 - 2.0 * local);
    float bottom = mix(hash(cell), hash(cell + vec2(1.0, 0.0)), blend.x);
    float top = mix(hash(cell + vec2(0.0, 1.0)), hash(cell + vec2(1.0, 1.0)), blend.x);
    return mix(bottom, top, blend.y);
}

float fbm(vec2 point) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 5; ++octave) {
        value += amplitude * noise(point);
        point = point * 2.03 + vec2(1.7, 9.2);
        amplitude *= 0.5;
    }
    return value;
}

float stars(vec2 point, float density, float radius) {
    vec2 grid = point * density;
    vec2 cell = floor(grid);
    float chance = hash(cell);
    if (chance > 0.14) {
        return 0.0;
    }
    vec2 offset = vec2(hash(cell + 7.1), hash(cell + 3.3)) - 0.5;
    float distance_to_star = length(fract(grid) - 0.5 - offset * 0.6);
    float twinkle = 0.6 + 0.4 * sin(time * (1.5 + chance * 25.0) + chance * 90.0);
    return smoothstep(radius, 0.0, distance_to_star) * twinkle;
}

vec3 nebula(vec2 uv) {
    vec2 drift = uv * 1.4 + vec2(time * 0.012, 0.0);
    float cloud = fbm(drift + fbm(drift * 1.6 + time * 0.004));
    float tint = fbm(drift * 0.6 + 4.0);
    vec3 deep_blue = vec3(0.04, 0.12, 0.38);
    vec3 violet = vec3(0.38, 0.07, 0.42);
    vec3 teal = vec3(0.02, 0.30, 0.32);
    vec3 hue = mix(mix(deep_blue, violet, smoothstep(0.35, 0.65, tint)), teal,
                   smoothstep(0.6, 0.8, tint));
    return hue * pow(cloud, 2.6) * 1.1;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution.y;
    vec3 color = vec3(0.006, 0.008, 0.03) + nebula(uv);
    for (int layer = 0; layer < 3; ++layer) {
        float depth = float(layer) + 1.0;
        vec2 scrolled = uv + vec2(time * 0.015 * depth * depth, depth * 17.3);
        float light = stars(scrolled, 90.0 / depth, 0.09);
        color += light * vec3(0.80, 0.86, 1.0) * (0.25 + 0.3 * depth);
    }
    vec2 centered = gl_FragCoord.xy / resolution - 0.5;
    color *= 1.0 - dot(centered, centered) * 0.7;
    finalColor = vec4(color, 1.0);
}
