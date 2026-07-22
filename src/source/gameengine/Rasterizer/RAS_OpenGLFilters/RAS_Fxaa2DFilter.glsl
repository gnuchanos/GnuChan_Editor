/**
Adapted FXAA code from:
https://www.geeks3d.com/20110405/fxaa-fast-approximate-anti-aliasing-demo-glsl-opengl-test-radeon-geforce/3/
*/

#define MIN_REDUCE 0.0078125
#define MUL_REDUCE 0.125
#define MAX_EXPAND 8.0
#define SUB_OFFSET 0.25

uniform sampler2D bgl_RenderedTexture;
uniform float bgl_RenderedTextureWidth;
uniform float bgl_RenderedTextureHeight;

vec4 AAPostFX(sampler2D tex, vec2 rcpRes, vec2 NW, vec2 NE, vec2 SW, vec2 SE, vec2 UV)
{
    vec3 lum = vec3(0.299, 0.587, 0.114);
    float lumNW = dot(texture2DLod(tex, NW, 0.0).xyz, lum);
    float lumNE = dot(texture2DLod(tex, NE, 0.0).xyz, lum);
    float lumSW = dot(texture2DLod(tex, SW, 0.0).xyz, lum);
    float lumSE = dot(texture2DLod(tex, SE, 0.0).xyz, lum);
    float lumCO = dot(texture2DLod(tex, UV, 0.0).xyz, lum);
    float lumMin = min(lumCO, min(min(lumNW, lumNE), min(lumSW, lumSE)));
    float lumMax = max(lumCO, max(max(lumNW, lumNE), max(lumSW, lumSE)));

    vec2 dir = vec2(-((lumNW + lumNE) - (lumSW + lumSE)),
									   ((lumNW + lumSW) - (lumNE + lumSE)));

    float dirReduce = max((lumNW + lumNE + lumSW + lumSE) *
						(0.25 * MUL_REDUCE), MIN_REDUCE);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2( MAX_EXPAND,  MAX_EXPAND),
		  max(vec2(-MAX_EXPAND, -MAX_EXPAND), dir * rcpDirMin)) * rcpRes;

    vec4 rgbA = 0.5 * (
        texture2DLod(tex, UV + dir * (1.0 / 3.0 - 0.5), 0.0) +
        texture2DLod(tex, UV + dir * (2.0 / 3.0 - 0.5), 0.0));
    vec4 rgbB = rgbA * 0.5 + 0.25 * (
        texture2DLod(tex, UV + dir * - 0.5, 0.0) +
        texture2DLod(tex, UV + dir *   0.5, 0.0));

    float lumB = dot(rgbB.rgb, lum);

    return (((lumB < lumMin) || (lumB > lumMax)) ? rgbA : rgbB);
}

void main()
{
	vec2 uv = gl_TexCoord[0].st;
    vec2 pxs = 1.0 / vec2(bgl_RenderedTextureWidth, bgl_RenderedTextureHeight);

    vec2 nw = uv - pxs * (0.5 + SUB_OFFSET);        // NW
    vec2 ne = nw + vec2(1.0, 0.0) * pxs;            // NE
    vec2 sw = nw + vec2(0.0, 1.0) * pxs;            // SW
    vec2 se = nw + vec2(1.0, 1.0) * pxs;            // SE

	gl_FragColor = AAPostFX(bgl_RenderedTexture, pxs, nw, ne, sw, se, uv);
}
