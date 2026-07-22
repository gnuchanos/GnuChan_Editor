
uniform sampler2D bgl_RenderedTexture;
uniform sampler2D bgl_DataTextures[1];
uniform sampler2D bgl_DepthTexture;

uniform vec3 ge_ssrparams; /* [step_max, bias, max_distance] */

float bias = ge_ssrparams.y * ge_ssrparams.x * 0.01;
float maxStepSize = ge_ssrparams.z / ge_ssrparams.x;

vec3 getViewPos(vec2 coord, float depth);
vec2 getProjCoord(vec3 coord);
vec2 getBinarySearch(vec3 dir, inout vec3 hitCoord, out float delta, out bool hit);
vec2 getRayCast(vec3 dir, vec3 hitCoord, out bool hit);
vec3 hash3D(vec3 co);
vec2 unpackFloat2(float f);
vec3 decode_octa(vec2 e);

vec2 texcoord = gl_TexCoord[0].st;

void main() {
	vec2 size = textureSize(bgl_RenderedTexture, 0);

	vec4 Gbuff0 = texelFetch(bgl_DataTextures[0], ivec2(texcoord * size), 0).rgba;

	vec3 normal = decode_octa(Gbuff0.rg);
	vec2 rough_metal = unpackFloat2(Gbuff0.b);

	float depth = texture(bgl_DepthTexture, texcoord).x;

	vec3 viewPos = getViewPos(texcoord, depth);
	vec3 viewDir = normalize(viewPos);
	vec3 reflectDir = reflect(viewDir, normal);

	vec4 worldPos = inverse(gl_ModelViewMatrix) * vec4(viewPos, 1.0);
	worldPos.xyz = worldPos.xyz / worldPos.w;

	vec3 noise = mix(vec3(0.0), hash3D(worldPos.xyz / worldPos.w), rough_metal.x) * 0.25;

	reflectDir += noise * rough_metal.x;

	bool hit = false;
	vec3 hitPos = viewPos;

	vec2 coord = getRayCast(reflectDir, hitPos, hit);

	vec3 sky = (rough_metal.y < 0.1) ? vec3(0.0) : vec3(0.5);

	vec3 color;

	if (hit) {
		color = texture(bgl_RenderedTexture, coord).rgb;
	} else {
		color = sky;
	}

	depth = 1.0 - step(1.0, depth);

	float fresnel = 1.0 - max(0.0, dot(-normal, viewDir));
	float mist = 1.0 - clamp(length(viewPos) / ge_ssrparams.z, 0.0, 1.0);

	gl_FragColor.rgb = color * depth * mist;
	gl_FragColor.a = fresnel;
}

vec3 hash3D(vec3 co) {
	co = fract(co * 0.8);
	co += dot(co, co.yxz + 19.19);

	return fract((co.xxy + co.yxx) * co.zyx) - 0.5;
}

vec2 unpackFloat2(float f) {
	return vec2(floor(f) / 255.0, fract(f));
}

vec3 decode_octa(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));

	if (v.z < 0)
		v.xy = (1.0 - abs(v.yx)) * vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);

	return -(gl_ModelViewMatrix * vec4(normalize(v), 0.0)).xyz;
}

vec3 getViewPos(vec2 coord, float depth) {
	vec3 ndc = vec3(coord * 2.0 - 1.0, depth);

	vec4 view_space = inverse(gl_ProjectionMatrix) * vec4(ndc, 1.0);
	view_space.xyz /= view_space.w;

	return view_space.xyz;
}

vec2 getProjCoord(vec3 coord) {
	vec4 projCoord = gl_ProjectionMatrix * vec4(coord, 1.0);
	projCoord.xy = projCoord.xy / projCoord.w * 0.5 + 0.5;

	return projCoord.xy;
}

vec2 getBinarySearch(vec3 dir, inout vec3 hitCoord, out float delta, out bool hit) {
    float depth;
    vec2 projCoord;
    vec3 viewSpace;
    float stepSize = maxStepSize;

    for (int i = 0; i < 8; i++) {
        projCoord = getProjCoord(hitCoord);
        depth = texture(bgl_DepthTexture, projCoord).x;
        viewSpace = getViewPos(projCoord, depth);
        delta = viewSpace.z - hitCoord.z;

        if (abs(delta) < bias) {
            hit = true;
            return projCoord;
        }

        stepSize = max(stepSize * 0.5, 0.001);

		if (delta < 0.0) {
            hitCoord += dir * stepSize;
        } else {
            hitCoord -= dir * stepSize;
        }
    }

    hit = false;
    return vec2(0.0);
}

vec2 getRayCast(vec3 dir, vec3 hitCoord, out bool hit) {
	float depth;
	float delta;
	vec2 projCoord;
	vec3 viewSpace;

	dir *= maxStepSize;

	for(int i = 0; i < int(ge_ssrparams.x); i++) {
		hitCoord += dir;
		projCoord = getProjCoord(hitCoord);

		if (projCoord.x <= 0.0 || projCoord.x >= 1.0 || projCoord.y <= 0.0 || projCoord.y >= 1.0) {
			return vec2(0.0);
		}

		depth = texture(bgl_DepthTexture, projCoord).x;
		viewSpace = getViewPos(projCoord, depth);
		delta = viewSpace.z - hitCoord.z; 

		if(delta > 0.0) {
			if (delta >= length(dir)) {
				hit = false;
				return vec2(0.0);
			} else {
				return getBinarySearch(dir, hitCoord, delta, hit);
			}
		}
	}

	hit = false;
	return vec2(0.0);
}

