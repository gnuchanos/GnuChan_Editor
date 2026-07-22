// coordinates on framebuffer in normalized (0.0-1.0) uv space
in vec4 uvcoord;

// color buffer
uniform sampler2D colorbuffer;

// jitter texture
uniform sampler2D jitter_tex;

// depth buffer
uniform sampler2D depthbuffer;

uniform vec4 ssr_params; // step_max, max_dist, roughness, bias

int num_samples = int(ssr_params.x);
float bias = ssr_params.x * ssr_params.w * 0.01;
float maxStepSize = ssr_params.y / ssr_params.x;
float roughness = ssr_params.z;

/* store the view space vectors for the corners of the view frustum here.
 * It helps to quickly reconstruct view space vectors by using uv coordinates,
 * see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 viewvecs[3];

vec3 getViewPos(vec2 coord, float depth);
vec2 getProjCoord(vec3 coord);
vec3 getNormalView(vec3 view);
vec2 getBinarySearch(vec3 dir, inout vec3 hitCoord, out float delta, out bool hit);
vec2 getRayCast(vec3 dir, vec3 hitCoord, out bool hit);

vec3 calculate_ssr_factor() {
	float depth = texture(depthbuffer, uvcoord.xy).x;

	//vec3 position = get_view_space_from_depth(uvcoord.xy, viewvecs[0].xyz, viewvecs[1].xyz, depth);

	vec3 viewPos = getViewPos(uvcoord.xy, depth);
	vec3 normal = getNormalView(viewPos);

	vec2 rotX = texture(jitter_tex, viewPos.xy * 43543.0).xy;
	vec3 noise = vec3(rotX.y, rotX.x, rotX.x - rotX.y);

	vec3 viewDir = normalize(viewPos);
	vec3 reflectDir = reflect(viewDir, normal);

	if (roughness > 0.05) {
		reflectDir += noise * roughness * roughness;
	}

	vec3 col = vec3(0.0);

	bool hit = false;
	vec3 hitPos = viewPos;

	vec2 coord = getRayCast(reflectDir, hitPos, hit);

	vec3 color = vec3(0.0);

	if (hit) {
		color = texture(colorbuffer, coord).rgb;
	}

	float fresnel = 1.0 - max(0.0, dot(normal, viewDir));

	depth = 1.0 - step(1.0, depth);

	float mist = 1.0 - clamp(length(viewPos) / ssr_params.y, 0.0, 1.0);

	return color * fresnel * depth * mist * (1.0 - roughness) * 0.25;
}

void main() {
	vec3 color = texture(colorbuffer, uvcoord.xy).rgb;

	vec3 final_color = calculate_ssr_factor();

	gl_FragColor.rgb = color + final_color;
	gl_FragColor.a = 1.0;
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

vec3 getNormalView(vec3 view) {
	vec3 normal = cross(
		normalize(dFdx(view)),
		normalize(dFdy(view))
	);

	return normalize(normal);
}

vec2 getBinarySearch(vec3 dir, inout vec3 hitCoord, out float delta, out bool hit) {
	float depth;
	vec2 projCoord;
	vec3 viewSpace;
	float stepSize = maxStepSize;

	for (int i = 0; i < 8; i++) {
		projCoord = getProjCoord(hitCoord);
		depth = texture(depthbuffer, projCoord).x;
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

	return getProjCoord(hitCoord);
}

vec2 getRayCast(vec3 dir, vec3 hitCoord, out bool hit) {
	float depth;
	float delta;
	vec2 projCoord;
	vec3 viewSpace;

	dir *= maxStepSize;

	for(int i = 0; i < num_samples; i++) {
		hitCoord += dir;
		projCoord = getProjCoord(hitCoord);

		if (projCoord.x <= 0.0 || projCoord.x >= 1.0 || projCoord.y <= 0.0 || projCoord.y >= 1.0) {
			return vec2(0.0);
		}

		depth = texture(depthbuffer, projCoord).x;
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

