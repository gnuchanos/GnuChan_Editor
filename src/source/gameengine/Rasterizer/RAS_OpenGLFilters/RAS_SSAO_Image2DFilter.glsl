
uniform sampler2D bgl_RenderedTexture;
uniform sampler2D bgl_RenderedBuffe;
uniform sampler2D bgl_DepthTexture;

const float kernelSamples = 6.0;
const float pixelSize = 2.5;
const float blurFalloff = 1.0 / 14.0;

float getLinearDepth(vec2 coord) {
    float depth = texture(bgl_DepthTexture, coord).x;

    vec3 ndc = vec3(coord, depth) * 2.0 - 1.0;

    vec4 viewSpace = inverse(gl_ProjectionMatrix) * vec4(ndc, 1.0);
    viewSpace.xyz /= viewSpace.w;

    return viewSpace.z;
}

vec2 pixel = 1.0 / textureSize(bgl_RenderedBuffe, 0);

#ifdef BLUR_X
    vec2 resolutionDir = vec2(pixel.x, 0.0);
#else
    vec2 resolutionDir = vec2(0.0, pixel.y);
#endif

vec4 blurFunction(vec2 uv, float r, float center_d, inout float w_total) {
    vec4 col = texture(bgl_RenderedBuffe, uv);
    float depth = getLinearDepth(uv);

    float diff = depth - center_d;
    diff = diff * diff * 12.0;

    float w = exp2(-r * r * blurFalloff - diff);
    w_total += w;

    return col * w;
}

vec2 texcoord = gl_TexCoord[0].st;

void main() {
    vec4 center_c = texture(bgl_RenderedBuffe, texcoord);
    float center_d = getLinearDepth(texcoord);

    vec4 c_total = center_c;
    float w_total = 1.0;

    for (float r = -kernelSamples; r <= -1.0; r++) {
        vec2 uv = texcoord + resolutionDir * r;
        c_total += blurFunction(uv, r, center_d, w_total);  
    }

    for (float r = 1.0; r <= kernelSamples; r++) {
        vec2 uv = texcoord + resolutionDir * r;
        c_total += blurFunction(uv, r, center_d, w_total);  
    }

    c_total /= w_total;

    #ifdef BLUR_X
        gl_FragColor = c_total;
    #else
		vec3 image = texture(bgl_RenderedTexture, texcoord).rgb;
		image += c_total.rgb;

		float lumina = dot(image, vec3(0.2126, 0.7152, 0.0722));
		c_total.a = mix(lumina, 1.0, c_total.a);

		gl_FragColor.rgb = image * c_total.a;
    #endif
}
