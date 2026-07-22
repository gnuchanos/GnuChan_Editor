/// UserCode fragment shader lib ///

vec2 getParallax(sampler2D tex, vec2 uv, float steps, float strength) {
	vec3 BITANGENT = cross(NORMAL, TANGENT.xyz);

	mat3 TBN = mat3(-TANGENT.xyz, BITANGENT, NORMAL);

	vec3 view = normalize(VIEW * TBN);

	vec2 delta = (view.xy / view.z) * strength / steps;
	vec2 coord = uv;

	float depth = 0.0;

	for (int i = 0; i < steps; i++) {
		if (texture(tex, coord).r < depth) {
			break;
		}

		coord -= delta;
		depth += 1.0 / steps;
	}

	return coord;
}

// normal tools //

vec3 computeNormal(vec3 vert) {
	return normalize(cross(dFdx(vert), dFdy(vert)));
}

vec3 normalMap(vec3 normal, float strength) {
	vec3 BITANGENT = cross(NORMAL, TANGENT.xyz);

	mat3 TBN = mat3(-TANGENT.xyz, BITANGENT, NORMAL);

	normal.xy *= strength;

	return normalize(normal * inverse(TBN));
}

// sprite tools //

vec4 _spriteSheet_fade(sampler2D tex, vec2 uv, int cols, int rows, float frame) {
	int index = int(frame);
	float blend = fract(frame);

	int prevCol = index % cols;
	int prevRow = index / cols;

	int nextCol = (index + 1) % cols;
	int nextRow = (index + 1) / cols;

	vec2 size = 1.0 / vec2(cols, rows);

	vec2 prevOffset = vec2(prevCol, prevRow) * size;
	vec2 nextOffset = vec2(nextCol, nextRow) * size;

	vec2 prevUV = uv * size + prevOffset;
	vec2 nextUV = uv * size + nextOffset;

	return mix(
		texture(tex, prevUV),
		texture(tex, nextUV),
		blend
	);
}

vec4 _spriteSheet(sampler2D tex, vec2 uv, int cols, int rows, float frame) {
	int index = int(frame);
	float blend = fract(frame);

	int prevCol = index % cols;
	int prevRow = index / cols;

	vec2 size = 1.0 / vec2(cols, rows);
	vec2 prevOffset = vec2(prevCol, prevRow) * size;
	vec2 prevUV = uv * size + prevOffset;

	return texture(tex, prevUV);
}

// texture tools //

vec4 textureSprite(sampler2D tex, vec2 uv, int cols, int rows, float frame, bool use) {
	if (use) {
		return _spriteSheet_fade(tex, uv, cols, rows, frame);
	} else {
		return _spriteSheet(tex, uv, cols, rows, frame);
	}
}

vec4 textureSprite(sampler2D tex, vec2 uv, int cols, int rows, float frame) {
	return _spriteSheet(tex, uv, cols, rows, frame);
}

vec4 textureSpherical(sampler2D tex, vec3 dir) {
	float u = atan(dir.y, dir.x) / (2.0 * 3.14159265) + 0.5;
	float v = asin(dir.z) / 3.14159265 + 0.5;

	return texture(tex, vec2(u, v));
}

vec4 textureMatcap(sampler2D tex, vec3 dir) {
	float m = 2.8284271247461903 * sqrt(dir.z + 1.0);

	return texture(tex, dir.xy / m + 0.5);
}

vec4 textureTriplanar(sampler2D tex, vec3 pos, float shap) {
	vec3 norm = inverse(gl_NormalMatrix) * NORMAL;
	vec3 blend = pow(abs(norm), vec3(shap));

	float sum = blend.x + blend.y + blend.z;
	blend /= sum;

	return texture(tex, pos.yz) * blend.x +
		   texture(tex, pos.xz) * blend.y +
		   texture(tex, pos.xy) * blend.z;
}

vec3 textureBump(sampler2D tex, vec2 uv) {
	vec2 pixel = 1.0 / textureSize(tex, 0);

	return vec3(texture(tex, uv).x - vec2(
		texture(tex, uv + pixel * vec2(1,0)).x,
		texture(tex, uv + pixel * vec2(0,1)).x
	), 0.5);
}

vec4 textureCube(sampler2D tex, vec3 dir) {
	vec3 box = abs(dir.xzy);
	vec2 uv;

	int face = (box.x >= box.y && box.x >= box.z) ? (dir.x > 0.0 ? 5 : 3):
			   (box.y >= box.z) ? (dir.z > 0.0 ? 1 : 0) : (dir.y > 0.0 ? 2 : 4);

	uv = (face == 5 || face == 3) ? vec2(-dir.y * sign(dir.x), dir.z) / box.x:
		 (face == 1 || face == 0) ? vec2(dir.x, -dir.y * sign(dir.z)) / box.y:
		 vec2(dir.x * sign(dir.y), dir.z) / box.z;

	uv = uv * 0.5 + 0.5;

	vec2 size = vec2(1.0 / 3.0, 1.0 / 2.0);
	vec2 offset = vec2(mod(float(face), 3.0), floor(float(face) / 3.0));

	uv = uv * size + offset * size;

	return texture2DLod(tex, uv, 0.0);
}

// outhers //

const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);

void alphaDither(float ap) {
	if (ap < 0.001 || ap < fract(magic.z * fract(dot(gl_FragCoord.xy, magic.xy)))) {
		discard;
	}
}

/// End UserCode fragment shader lib ///
