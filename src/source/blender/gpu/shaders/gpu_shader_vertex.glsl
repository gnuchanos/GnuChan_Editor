#ifdef USE_OPENSUBDIV
in vec3 normal;
in vec4 position;

out block {
	VertexData v;
} outpt;
#endif

#if __VERSION__ < 130
  #define in attribute
  #define flat
  #define out varying
#endif

#ifdef USE_INSTANCING
in mat3 ininstmatrix;
in vec3 ininstposition;
in vec4 ininstcolor;
in int ininstlayer;
in vec3 ininstinfo;

out vec4 varinstcolor;
out mat4 varinstmat;
out mat4 varinstinvmat;
flat out int varinstlayer;
out vec3 varinstinfo;

#endif

uniform mat4 unfviewmat;
uniform mat4 unfobmat;

uniform float unftime;

out vec3 varposition;
out vec3 varnormal;
out float varvertexid;

#if __VERSION__ < 130
  #undef in
  #undef flat
  #undef out
#endif

#ifdef CLIP_WORKAROUND
varying float gl_ClipDistance[6];
#endif

#ifdef USE_USER_CODE
uniform vec4 OBJECT_COLOR;
#endif

#ifdef USE_FOLIAGE
uniform vec3 unfoliageparams; // strength, time * turbulence, is_grass

float foliage_random(in vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

float foliage_noise(in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    float a = foliage_random(i);
    float b = foliage_random(i + vec2(1.0, 0.0));
    float c = foliage_random(i + vec2(0.0, 1.0));
    float d = foliage_random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void foliage_wind(in vec4 position, in float time, in float strength, in float grass, out vec4 transpos)
{
    transpos = position;

	float wind = foliage_noise(transpos.xy + time * 1.0);

	transpos.xy += vec2(wind * strength, -wind * strength) * ((grass == 1) ? transpos.z : 1.0);
}
#endif

/* Color, keep in sync with: gpu_shader_vertex_world.glsl */

float srgb_to_linearrgb(float c)
{
	if (c < 0.04045)
		return (c < 0.0) ? 0.0 : c * (1.0 / 12.92);
	else
		return pow((c + 0.055) * (1.0 / 1.055), 2.4);
}

void srgb_to_linearrgb(vec3 col_from, out vec3 col_to)
{
	col_to.r = srgb_to_linearrgb(col_from.r);
	col_to.g = srgb_to_linearrgb(col_from.g);
	col_to.b = srgb_to_linearrgb(col_from.b);
}

void srgb_to_linearrgb(vec4 col_from, out vec4 col_to)
{
	col_to.r = srgb_to_linearrgb(col_from.r);
	col_to.g = srgb_to_linearrgb(col_from.g);
	col_to.b = srgb_to_linearrgb(col_from.b);
	col_to.a = col_from.a;
}

bool is_srgb(int info)
{
#ifdef USE_NEW_SHADING
	return (info == 1)? true: false;
#else
	return false;
#endif
}

void set_var_from_attr(float attr, int info, out float var)
{
	var = attr;
}

void set_var_from_attr(vec2 attr, int info, out vec2 var)
{
	var = attr;
}

void set_var_from_attr(vec3 attr, int info, out vec3 var)
{
	if (is_srgb(info)) {
		srgb_to_linearrgb(attr, var);
	}
	else {
		var = attr;
	}
}

void set_var_from_attr(vec4 attr, int info, out vec4 var)
{
	if (is_srgb(info)) {
		srgb_to_linearrgb(attr, var);
	}
	else {
		var = attr;
	}
}

/* end color code */

/* inputs for user code */
vec3 VERTEX = gl_Vertex.xyz;
vec3 NORMAL = gl_Normal;
float TIME = unftime;

void vertex(); /* declare here but user provides definition. */

void main()
{
#ifndef USE_OPENSUBDIV
	vec4 position = gl_Vertex;
	vec3 normal = gl_Normal;
#endif

#ifdef USE_FOLIAGE
	foliage_wind(position, unfoliageparams[1], unfoliageparams[0], unfoliageparams[2], position);
#endif

#ifdef USE_INSTANCING
	mat4 instmat = mat4(vec4(ininstmatrix[0], ininstposition.x),
						vec4(ininstmatrix[1], ininstposition.y),
						vec4(ininstmatrix[2], ininstposition.z),
						vec4(0.0, 0.0, 0.0, 1.0));

	varinstmat = transpose(instmat);
#if !defined(GPU_ATI)
	varinstinvmat = inverse(varinstmat);
#else
	varinstinvmat = varinstmat;
#endif
	varinstcolor = ininstcolor;
	varinstlayer = ininstlayer;
	varinstinfo = ininstinfo;

	position *= instmat;
	normal *= ininstmatrix;
#endif

	VERTEX = position.xyz;
	varvertexid = float(gl_VertexID);

#ifdef USE_USER_CODE
	/* for user code */
	vertex();
#endif

	vec4 co = gl_ModelViewMatrix * vec4(VERTEX, position.w);

	varposition = co.xyz;
	varnormal = normalize(gl_NormalMatrix * normal);
	

	gl_Position = gl_ProjectionMatrix * co;

#ifdef CLIP_WORKAROUND
	int i;
	for (i = 0; i < 6; i++)
		gl_ClipDistance[i] = dot(co, gl_ClipPlane[i]);
#elif !defined(GPU_ATI)
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = co;
#endif

#ifdef USE_OPENSUBDIV
	outpt.v.position = co;
	outpt.v.normal = varnormal;
#endif
