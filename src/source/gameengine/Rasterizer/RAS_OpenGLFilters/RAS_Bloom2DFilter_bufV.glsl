uniform sampler2D bgl_RenderedBloomH;
uniform vec4 ge_BloomParams; // intensity, threshold, width, height

vec2 texcoord = gl_TexCoord[0].st;

const float weights[11] = float[](
	0.0093, 0.028002, 0.065984, 0.121703, 0.175713,
	0.198596, 0.175713, 0.121703, 0.065984, 0.028002, 0.0093
);

void main() {
	const float pixel = 5.0 / ge_BloomParams.w;

	vec3 result = vec3(0.0);

	for (int i = -5; i <= 5; i++) {
		vec2 offset = vec2(0.0, pixel * i);

		result += texture(bgl_RenderedBloomH, texcoord + offset).rgb * weights[i + 5];
	}

	gl_FragColor.rgb = result;
}
