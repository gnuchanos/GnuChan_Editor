//#define GIAO

uniform sampler2D bgl_RenderedTexture;
uniform sampler2D bgl_DataTextures[2];
uniform sampler2D bgl_DepthTexture;
uniform vec4 ge_ssaoparams; // Samples, Strength, Distance, Attenuation
uniform vec2 ge_ssaoGIirradiance; // SSAOGI: Use_gi, Irradiance

float radius = ge_ssaoparams.z;
float aoCutoff = ge_ssaoparams.w;
float invSamples = 1.0 / ge_ssaoparams.x;
float turnSpiral = 6.2831 * 5.0;
float twoSamples = radius * 2.0 * invSamples;
float contrastK = ge_ssaoparams.y;

vec3 decode_octa(vec2 e);
vec2 unpackFloat2(float f);
vec3 getViewPosition(vec2 coord);
vec2 tapLocation(float sampleNumber, float spinAngle);

float bayer2(vec2 a){
    a = floor(a);
    return fract(dot(a, vec2(0.5, a.y * 0.75)));
}

float bayer4(vec2 a) {return bayer2(0.5 * a) * 0.25 + bayer2(a);}
float bayer8(vec2 a) {return bayer4(0.5 * a) * 0.25 + bayer2(a);}

vec2 texcoord = gl_TexCoord[0].st;

void main() {
	vec2 size = textureSize(bgl_DepthTexture, 0);
	vec2 coord = texcoord * size;

	vec4 Gbuff0 = texelFetch(bgl_DataTextures[0], ivec2(coord), 0).rgba;

	vec3 viewPos = getViewPosition(texcoord);
	vec3 normal = decode_octa(Gbuff0.rg);
	vec2 diff_spec = unpackFloat2(Gbuff0.a);

	float ssDiskRadius = radius / max(viewPos.z, 0.2);

	float aoBias = 0.002;
	aoBias *= -viewPos.z;

	float patternRotationAngle = bayer8(gl_FragCoord.xy) * 396.5;

	float ao = 0.0;
	vec3 gi = vec3(0.0);
	float epsilon = 0.001;

	for(int i = 0; i < int(ge_ssaoparams.x); ++i) {
		vec2 unitOffset = tapLocation(float(i), patternRotationAngle);

		vec2 sampleCoord = texcoord + unitOffset * ssDiskRadius;

		float H  = sampleCoord.x > 1.0 ? 0.0 : 1.0;
			  H *= sampleCoord.x < 0.0 ? 0.0 : 1.0;
			  H *= sampleCoord.y > 1.0 ? 0.0 : 1.0;
			  H *= sampleCoord.y < 0.0 ? 0.0 : 1.0;

		vec3 Q = getViewPosition(sampleCoord);

		vec3 v = Q - viewPos;
		float vv = dot(v, v);
		float vn = dot(v, normal);

		H *= vv > radius ? 0.0 : 1.0;
		float temp = (max(0.0, vn + aoBias) * H) / (epsilon + vv);

		vec3 col = texture(bgl_RenderedTexture, sampleCoord).rgb;

		float lum = dot(col, col);

		ao += temp;
		gi += col * temp * pow(lum, 4.0);
	}

	ao *= twoSamples;
	ao = 1.0 - clamp(ao * contrastK, 0.0, 1.0);
	gi /= ge_ssaoparams.x;

	float depth = texture(bgl_DepthTexture, texcoord).x;

	// SSAOGI
	if (ge_ssaoGIirradiance[0] == 1.0) {
		vec3 albedo = texture(bgl_DataTextures[1], texcoord).rgb;

		gl_FragColor.rgb = gi * albedo * ge_ssaoGIirradiance[1];
	}
	else {
		gl_FragColor.rgb = vec3(0.0);
	}

	gl_FragColor.a = clamp(ao + step(1.0, depth), 1.0 - diff_spec.x, 1.0);
}

vec3 decode_octa(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));

	if (v.z < 0)
		v.xy = (1.0 - abs(v.yx)) * vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);

	return (gl_ModelViewMatrix * vec4(normalize(v), 0.0)).xyz;
}

vec2 unpackFloat2(float f) {
	return vec2(floor(f) / 255.0, fract(f));
}

vec3 getViewPosition(vec2 coord) {
	float depth = texture(bgl_DepthTexture, coord).x;
	vec3 ndc = vec3(coord, depth) * 2.0 - 1.0;

	vec4 view_space = inverse(gl_ProjectionMatrix) * vec4(ndc, 1.0);
	view_space.xyz /= view_space.w;

	return -view_space.xyz;
}

vec2 tapLocation(float sampleNumber, float spinAngle) {
	float alpha = (sampleNumber + 0.5) * invSamples;
	float angle = alpha * turnSpiral + spinAngle;

	return vec2(cos(angle), sin(angle)) * alpha;
}
