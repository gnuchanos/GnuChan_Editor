
uniform sampler2D bgl_RenderedTexture;
uniform sampler2D bgl_DepthTexture;

uniform vec2 ge_LightScatterSunPos;
uniform vec4 ge_LightScatterParams; // step_max, step_size, threshold, intensity

vec3 saturation(vec3 col) {
	const vec3 W = vec3(0.2125, 0.7154, 0.0721);

	vec3 intensity = vec3(dot(col, W));

	return mix(intensity, col, 0.5);
}

vec2 uvcoord = gl_TexCoord[0].xy;

void main() {
	vec2 delta = (uvcoord - ge_LightScatterSunPos) * ge_LightScatterParams.y;

	vec3 result = vec3(0.0);
	float weights = 1.0;

	for (int i = 0; i < ge_LightScatterParams.x; i++) {
		vec2 offset = uvcoord - delta * (float(i) / ge_LightScatterParams.x);

		vec3 color = texture(bgl_RenderedTexture, offset).rgb - ge_LightScatterParams.z;

		float depth = texture(bgl_DepthTexture, offset).x;
		depth = step(1.0, depth);

		weights *= 0.9;
		result += max(vec3(0.0), color * depth) * weights;
	}

	float ocluder = max(0.0, 1.0 - distance(ge_LightScatterSunPos, vec2(0.5)));

	gl_FragColor.rgb = saturation(result) * ocluder * ge_LightScatterParams.w;
	gl_FragColor.a = 1.0;
}
