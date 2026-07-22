
uniform sampler2D bgl_RenderedTexture;
uniform vec4 ge_BloomParams; // intensity, threshold, width, height

vec2 texcoord = gl_TexCoord[0].st;

void main() {
	vec3 color = texture(bgl_RenderedTexture, texcoord).rgb;
	vec3 result = max(color - ge_BloomParams.y, 0.0) * ge_BloomParams.x;

	gl_FragColor = vec4(result, 1.0);
}
