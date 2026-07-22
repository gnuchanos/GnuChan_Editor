
uniform sampler2D ssr_buffer;
uniform sampler2D bgl_DataTextures[1];
uniform sampler2D bgl_RenderedTexture;

vec2 unpackFloat2(float f);
float gaussianWeight(int index, float sigma);

vec2 texcoord = gl_TexCoord[0].st;

void main() {
	#ifndef BLUR_X
		vec4 Gbuff0 = texelFetch(bgl_DataTextures[0], ivec2(gl_FragCoord.xy), 0).rgba;
	#else
		vec4 Gbuff0 = texture(bgl_DataTextures[0], texcoord).rgba;
	#endif

	vec2 rough_metal = unpackFloat2(Gbuff0.b);
	vec2 diff_spec = unpackFloat2(Gbuff0.a);

	vec2 pixel = rough_metal.x / textureSize(bgl_DataTextures[0], 0);

	#ifndef BLUR_X
		vec2 offset = vec2(pixel.x * 4.0, 0.0);
	#else
		vec2 offset = vec2(0.0, pixel.y * 4.0);
	#endif

	vec3 result = vec3(0.0);
	float sigma = 2.0;
	float total = 0.0;

	vec2 size = textureSize(ssr_buffer, 0);

	for (int i = -5; i <= 5; i++) {
		float weight = gaussianWeight(i, sigma);
		vec2 coord = texcoord + offset * i;

		result += texture(ssr_buffer, coord).rgb * weight;
		total += weight;
	}

	result /= total;

	float fresnel = texture(ssr_buffer, texcoord).a;

	#ifdef BLUR_X
		gl_FragColor.rgb = result;
		gl_FragColor.a = fresnel;
	#else
		vec3 image = texture(bgl_RenderedTexture, texcoord).rgb;

		float factor = fresnel * diff_spec.y * (1.0 - rough_metal.x);

		if (rough_metal.y < 0.1) {
			gl_FragColor.rgb = image + result * factor;
		} else {
			gl_FragColor.rgb = mix(image, image * result * 2.0, length(result));
		}
	#endif
}

vec2 unpackFloat2(float f) {
	return vec2(floor(f) / 255.0, fract(f));
}

float gaussianWeight(int index, float sigma) {
    return exp(-float(index * index) / (2.0 * sigma * sigma));
}
