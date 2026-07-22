
uniform sampler2D bgl_RenderedTexture;

uniform vec3 ge_TonemapParams; // Tonemap Type, Shader Exposure, Gamma
uniform vec2 ge_toneColorParams; // Saturation, Temperature

/* Uncharted Tonemap */
const vec4 TonemapParams = vec4(0.15, 0.55, 0.15, 0.25); // A, B, C, D
const vec2 TonemapExtras = vec2(0.05, 0.30); // E, F

vec3 Uncharted2Tonemap(vec3 x) {
	vec4 p = TonemapParams;
    vec2 e = TonemapExtras;

	return ((x * (p.x * x + p.z * p.y) + p.w * e.x) / 
            (x * (p.x * x + p.y) + p.w * e.y)) - e.x / e.y;
}

const mat3 m1 = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

const mat3 m2 = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);

vec3 aces_tonemap(vec3 color) { // gamma 2.2 | exposure 1.0
    color *= ge_TonemapParams.y;

    vec3 v = m1 * color;    
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;

    return pow(clamp(m2 * (a / b), 0.0, 1.0), vec3(1.0 / ge_TonemapParams.z));
}

vec3 sRGBToLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));
}

vec3 applyAGX(vec3 color) {
    color *= ge_TonemapParams.y;

    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;

    color = (color * (a * color + b)) / (color * (c * color + d) + e);

    return pow(clamp(color, 0.0, 1.0), vec3(1.0 / ge_TonemapParams.z));
}

vec3 linearToSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

vec3 filmicTonemap(vec3 color) {
    color *= ge_TonemapParams.y;

    // Filmic Tonemapping
    color = max(vec3(0.0), color - 0.004);
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);

    return pow(clamp(color, 0.0, 1.0), vec3(1.0 / ge_TonemapParams.z));
}

vec3 setSaturation(vec3 color, float factor) {
    float gray = dot(color, vec3(0.2989, 0.5870, 0.1140));
    return mix(vec3(gray), color, factor);
}

vec3 setTemperature(vec3 color, float temp) {
    vec3 warmth = mix(vec3(0.5, 0.6, 1.0), vec3(1.0, 0.8, 0.5), temp);
    return mix(color * warmth, color, 0.5 * (sin(temp * 3.1415) + 1.0));
}

void main() {
	vec2 uv = gl_TexCoord[0].xy;
	vec4 color = texture(bgl_RenderedTexture, uv);

	// Uncharted Tonemap
	if (ge_TonemapParams.x == 0.0) {
		vec3 rgb = color.rgb;
		rgb *= max(ge_TonemapParams.y * 12.70, 0.001);

		vec3 white_point = vec3(28.0);
		vec3 tonemapped = Uncharted2Tonemap(rgb);
		vec3 white_tonemapped = Uncharted2Tonemap(white_point);

		tonemapped = max(tonemapped / max(white_tonemapped, vec3(0.001)), vec3(0.0));
		rgb = pow(tonemapped, vec3(6.0 / max(ge_TonemapParams.z, 0.001)));

		gl_FragColor = vec4(rgb, color.a);
	}
	// ACES
	else if (ge_TonemapParams.x == 1.0) {
		gl_FragColor = vec4(aces_tonemap(color.rgb), color.a);
	}
	// AGX
	else if (ge_TonemapParams.x == 2.0) {
		vec3 linearColor = sRGBToLinear(color.rgb);
		vec3 agxColor = applyAGX(linearColor);
		gl_FragColor = vec4(linearToSRGB(agxColor), color.a);
	}
	// Filmic
	else if (ge_TonemapParams.x == 3.0) {
		gl_FragColor = vec4(filmicTonemap(color.rgb), color.a);
	}

	/* Temperature and Saturation */
	gl_FragColor.rgb = setTemperature(gl_FragColor.rgb, ge_toneColorParams.y); // [0.0, 1.0]
	gl_FragColor.rgb = setSaturation(gl_FragColor.rgb, ge_toneColorParams.x); // [0.0, 2.0]

}
