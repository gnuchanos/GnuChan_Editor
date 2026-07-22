
in vec4 uvcoord;

uniform sampler2D colorbuffer;
uniform vec2 vignetteParams; // size = 15.0, radius = 0.25

void main(void) {
	vec2 uv = uvcoord.xy;
	uv *=  1.0 - uv.yx;

	float vig = pow(uv.x * uv.y * vignetteParams.x * 10.0, vignetteParams.y);

    vec3 image = texture(colorbuffer, uvcoord.xy).rgb;

	gl_FragColor.rgb = image * clamp(vig, 0.0, 1.0);
}
