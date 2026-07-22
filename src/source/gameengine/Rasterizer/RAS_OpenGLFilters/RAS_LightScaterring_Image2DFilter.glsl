
uniform sampler2D bgl_RenderedTexture;
uniform sampler2D bgl_LightScatter;

vec2 texcoord  = gl_TexCoord[0].st;

void main() {
	vec3 image = texture(bgl_RenderedTexture, texcoord).rgb;
	vec3 scatter = texture(bgl_LightScatter,  texcoord).rgb;

	gl_FragColor.rgb = image + scatter;
}
