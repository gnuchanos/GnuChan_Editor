
uniform sampler2D bgl_RenderedTexture;

uniform vec2 ge_vignetteParams; // size = 15.0, radius = 0.25

vec2 texcoord = gl_TexCoord[0].st;

void main(void) {
	vec2 uv = texcoord;
	uv *=  1.0 - uv.yx;

	float vig = pow(uv.x * uv.y * ge_vignetteParams.x * 10.0, ge_vignetteParams.y);

	vec3 image = texture(bgl_RenderedTexture, texcoord).rgb;

	gl_FragColor.rgb = image * clamp(vig, 0.0, 1.0);
}
