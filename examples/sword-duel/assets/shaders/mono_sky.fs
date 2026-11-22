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
    if (chance > 0.1) {
        return 0.0;
    }
    vec2 offset = vec2(hash(cell + 7.1), hash(cell + 3.3)) - 0.5;
    float distance_to_star = length(fract(grid) - 0.5 - offset * 0.6);
    float twinkle = 0.5 + 0.5 * sin(time * (2.0 + chance * 30.0) + chance * 90.0);
    return smoothstep(radius, 0.0, distance_to_star) * twinkle;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution.y;
    vec2 drift = uv * 1.3 + vec2(time * 0.05, 0.0);
    float cloud = fbm(drift + fbm(drift * 1.5 + time * 0.03));
    float grey = pow(cloud, 2.2) * 0.55 + 0.02;

    float sweep = sin(uv.x * 2.1 - time * 0.09) * 0.5 + 0.5;
    grey += sweep * 0.05 * (1.0 - uv.y);

    float glow = 0.0;
    for (int layer = 0; layer < 3; ++layer) {
        float depth = float(layer) + 1.0;
        vec2 scrolled = uv + vec2(time * 0.04 * depth * depth, depth * 17.3);
        glow += stars(scrolled, 85.0 / depth, 0.1) * (0.3 + 0.35 * depth);
    }

    float shade = grey + glow;
    vec2 centered = gl_FragCoord.xy / resolution - 0.5;
    shade *= 1.0 - dot(centered, centered) * 0.6;
    finalColor = vec4(vec3(shade), 1.0);
}
