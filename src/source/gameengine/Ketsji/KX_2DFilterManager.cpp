/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Ulysse Martin, Tristan Porteries.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_2DFilterManager.cpp
 *  \ingroup ketsji
 */

#include "KX_2DFilterManager.h"
#include "RAS_ICanvas.h"

#include "MEM_guardedalloc.h"

#include "CM_Message.h"

extern "C" {
	extern char datatoc_RAS_Bloom2DFilter_buf_glsl[];
	extern char datatoc_RAS_Bloom2DFilter_bufH_glsl[];
	extern char datatoc_RAS_Bloom2DFilter_bufV_glsl[];
	extern char datatoc_RAS_Bloom2DFilter_Image_glsl[];

	extern char datatoc_RAS_SSR2DFilter_glsl[];
	extern int datatoc_RAS_SSR_Blur2DFilter_glsl_size;
	extern char datatoc_RAS_SSR_Blur2DFilter_glsl[];

	extern char datatoc_RAS_LightScaterring_Buffer2DFilter_glsl[];
	extern char datatoc_RAS_LightScaterring_Image2DFilter_glsl[];

	extern int datatoc_RAS_SSAO_Image2DFilter_glsl_size;
	extern char datatoc_RAS_SSAO_Image2DFilter_glsl[];
	
	extern int datatoc_RAS_SSAO_Buffer2DFilter_glsl_size;
	extern char datatoc_RAS_SSAO_Buffer2DFilter_glsl[];
  }

KX_2DFilterManager::KX_2DFilterManager(RAS_ICanvas *canvas, BuildInFilters filters) :
	RAS_2DFilterManager(filters)
{
	/* This location doesn't seem very good to me but it works fine here, we need to generate the KX_2DFilter to have offscreen and not RAS_* */
	/* Only for Range legacy, the code can be deprecated after Range 2.0+ */
	if (filters.useBloom) {
		// This bloom shader uses 8 passIndex. eg [2 ~ 9].
		int filterPassIndex = FILTERPASS_BLOOM_BUFFER;

		// pass buffer
		KX_2DFilter *bloomB0 = BloomPass(filters, 0, filterPassIndex, 2, canvas->GetWidth(), canvas->GetHeight());

		// pass 0
		KX_2DFilter *bloomH0 = BloomPass(filters, 1, filterPassIndex + 1, 2, canvas->GetWidth(), canvas->GetHeight());
		KX_2DFilter *bloomV0 = BloomPass(filters, 2, filterPassIndex + 2, 2, canvas->GetWidth(), canvas->GetHeight());

		bloomH0->SetTexture(0, bloomB0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomB");
		bloomV0->SetTexture(0, bloomH0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomH");

		// pass 1
		KX_2DFilter *bloomH1 = BloomPass(filters, 1, filterPassIndex + 3, 4, canvas->GetWidth(), canvas->GetHeight());
		KX_2DFilter *bloomV1 = BloomPass(filters, 2, filterPassIndex + 4, 4, canvas->GetWidth(), canvas->GetHeight());

		bloomH1->SetTexture(0, bloomB0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomB");
		bloomV1->SetTexture(0, bloomH1->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomH");

		// pass 2
		KX_2DFilter *bloomH2 = BloomPass(filters, 1, filterPassIndex + 5, 8, canvas->GetWidth(), canvas->GetHeight());
		KX_2DFilter *bloomV2 = BloomPass(filters, 2, filterPassIndex + 6, 8, canvas->GetWidth(), canvas->GetHeight());

		bloomH2->SetTexture(0, bloomB0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomB");
		bloomV2->SetTexture(0, bloomH2->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomH");

		// pass final
		RAS_2DFilterData bloomData;
		bloomData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		bloomData.filterPassIndex = FILTERPASS_BLOOM_IMAGE;
		bloomData.gameObject = nullptr;
		bloomData.mipmap = false;
		bloomData.propertyNames = {};
		bloomData.buildInFilters = filters;
		bloomData.shaderText = datatoc_RAS_Bloom2DFilter_Image_glsl;

		KX_2DFilter *bloomI = static_cast<KX_2DFilter *>(AddFilter(bloomData, true));

		bloomI->SetTexture(0, bloomV0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomI0");
		bloomI->SetTexture(1, bloomV1->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomI1");
		bloomI->SetTexture(2, bloomV2->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomI2");
		bloomI->SetTexture(3, bloomB0->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBloomB");
	}

	// Need to be done before bloom passes
	if (filters.useSSR) {
		// Buffer
		RAS_2DFilterData ssrData;
		ssrData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssrData.filterPassIndex = FILTERPASS_SSR_BUFFER;
		ssrData.gameObject = nullptr;
		ssrData.mipmap = false;
		ssrData.propertyNames = {};
		ssrData.buildInFilters = filters;
		ssrData.shaderText = datatoc_RAS_SSR2DFilter_glsl;

		KX_2DFilter *SSR = static_cast<KX_2DFilter *>(AddFilter(ssrData, true));
		KX_2DFilterOffScreen *offScreen = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (canvas->GetWidth() / filters.ssr_lod), (canvas->GetHeight() / filters.ssr_lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_NONE);
		SSR->SetOffScreen(offScreen);

		// Blur X
		RAS_2DFilterData ssrBlurX;
		ssrBlurX.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssrBlurX.filterPassIndex = FILTERPASS_SSR_BLUR;
		ssrBlurX.gameObject = nullptr;
		ssrBlurX.mipmap = false;
		ssrBlurX.propertyNames = {};
		ssrBlurX.buildInFilters = filters;

		const char *define = "#define BLUR_X\n";

		char *ssrShaderX = (char *)MEM_mallocN(datatoc_RAS_SSR_Blur2DFilter_glsl_size + 15, "blurx");
		strcpy(ssrShaderX, define);
		strcat(ssrShaderX, datatoc_RAS_SSR_Blur2DFilter_glsl);
		ssrBlurX.shaderText = ssrShaderX;
		MEM_freeN(ssrShaderX);

		KX_2DFilter *SSR_X = static_cast<KX_2DFilter *>(AddFilter(ssrBlurX, true));
		KX_2DFilterOffScreen *offScreenX = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (canvas->GetWidth() / filters.ssr_lod), (canvas->GetHeight() / filters.ssr_lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_NONE);
		SSR_X->SetOffScreen(offScreenX);
		SSR_X->SetTexture(0, SSR->GetOffScreen()->GetColorBindCode(0), "ssr_buffer");

		// Image
		RAS_2DFilterData ssrblurData;
		ssrblurData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssrblurData.filterPassIndex = FILTERPASS_SSR_IMAGE;
		ssrblurData.gameObject = nullptr;
		ssrblurData.mipmap = false;
		ssrblurData.propertyNames = {};
		ssrblurData.buildInFilters = filters;
		ssrblurData.shaderText = datatoc_RAS_SSR_Blur2DFilter_glsl;

		KX_2DFilter *SSR_Blur = static_cast<KX_2DFilter *>(AddFilter(ssrblurData, true));

		SSR_Blur->SetTexture(0, SSR_X->GetOffScreen()->GetColorBindCode(0), "ssr_buffer");
	}

	if (filters.useLightScatter) {
		// Buffer
		RAS_2DFilterData scatterData;
		scatterData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		scatterData.filterPassIndex = FILTERPASS_LIGHTSCATTER;
		scatterData.gameObject = nullptr;
		scatterData.mipmap = false;
		scatterData.propertyNames = {};
		scatterData.buildInFilters = filters;
		scatterData.shaderText = datatoc_RAS_LightScaterring_Buffer2DFilter_glsl;

		KX_2DFilter *Scatter = static_cast<KX_2DFilter *>(AddFilter(scatterData, true));
		KX_2DFilterOffScreen *offScreen = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (canvas->GetWidth() / filters.scatter_lod), (canvas->GetHeight() / filters.scatter_lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_HALF_FLOAT);
		Scatter->SetOffScreen(offScreen);

		// Image
		RAS_2DFilterData scatterImageData;
		scatterImageData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		scatterImageData.filterPassIndex = FILTERPASS_LIGHTSCATTER + 1;
		scatterImageData.gameObject = nullptr;
		scatterImageData.mipmap = false;
		scatterImageData.propertyNames = {};
		scatterImageData.buildInFilters = filters;
		scatterImageData.shaderText = datatoc_RAS_LightScaterring_Image2DFilter_glsl;

		KX_2DFilter *Scatter_Image = static_cast<KX_2DFilter *>(AddFilter(scatterImageData, true));

		Scatter_Image->SetTexture(0, Scatter->GetOffScreen()->GetColorBindCode(0), "bgl_LightScatter");
	}

	if (filters.useSSAO) {
		// Buffer
		RAS_2DFilterData ssaoBuffer;
		ssaoBuffer.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssaoBuffer.filterPassIndex = FILTERPASS_SSAO_BUFFER;
		ssaoBuffer.gameObject = nullptr;
		ssaoBuffer.mipmap = false;
		ssaoBuffer.propertyNames = {};
		ssaoBuffer.buildInFilters = filters;
		ssaoBuffer.shaderText = datatoc_RAS_SSAO_Buffer2DFilter_glsl;

		if (filters.use_SSAO_GI) {
			char *shaderSSAOGI = (char *)MEM_mallocN(datatoc_RAS_SSAO_Buffer2DFilter_glsl_size + 14, "SSAOGI");

			const char *defineGI = "#define GIAO\n";
			strcpy(shaderSSAOGI, defineGI);
			strcat(shaderSSAOGI, datatoc_RAS_SSAO_Buffer2DFilter_glsl);
			ssaoBuffer.shaderText = shaderSSAOGI;
			MEM_freeN(shaderSSAOGI);
		}

		KX_2DFilter *SSAO = static_cast<KX_2DFilter *>(AddFilter(ssaoBuffer, true));
		KX_2DFilterOffScreen *offScreen = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (canvas->GetWidth() / filters.ssao_lod), (canvas->GetHeight() / filters.ssao_lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_NONE);
		SSAO->SetOffScreen(offScreen);

		// Blur X
		RAS_2DFilterData ssaoBlurX;
		ssaoBlurX.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssaoBlurX.filterPassIndex = FILTERPASS_SSAO_BLUR;
		ssaoBlurX.gameObject = nullptr;
		ssaoBlurX.mipmap = false;
		ssaoBlurX.propertyNames = {};
		ssaoBlurX.buildInFilters = filters;

		const char *define = "#define BLUR_X\n";

		char *shaderBlurX = (char*)MEM_mallocN(datatoc_RAS_SSAO_Image2DFilter_glsl_size + 17, "blurx");
		strcpy(shaderBlurX, define);
		strcat(shaderBlurX, datatoc_RAS_SSAO_Image2DFilter_glsl);
		ssaoBlurX.shaderText = shaderBlurX;
		MEM_freeN(shaderBlurX);

		KX_2DFilter *SSAO_X = static_cast<KX_2DFilter *>(AddFilter(ssaoBlurX, true));
		KX_2DFilterOffScreen *offScreenX = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (canvas->GetWidth() / filters.ssao_lod), (canvas->GetHeight() / filters.ssao_lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_NONE);
		SSAO_X->SetOffScreen(offScreenX);
		SSAO_X->SetTexture(0, SSAO->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBuffe");

		// Image
		RAS_2DFilterData ssaoImageData;
		ssaoImageData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
		ssaoImageData.filterPassIndex = FILTERPASS_SSAO_IMAGE;
		ssaoImageData.gameObject = nullptr;
		ssaoImageData.mipmap = false;
		ssaoImageData.propertyNames = {};
		ssaoImageData.buildInFilters = filters;
		ssaoImageData.shaderText = datatoc_RAS_SSAO_Image2DFilter_glsl;

		KX_2DFilter *SSAO_Image = static_cast<KX_2DFilter *>(AddFilter(ssaoImageData, true));

		SSAO_Image->SetTexture(0, SSAO_X->GetOffScreen()->GetColorBindCode(0), "bgl_RenderedBuffe");
	}
}


KX_2DFilterManager::~KX_2DFilterManager()
{
}

KX_2DFilter *KX_2DFilterManager::BloomPass(BuildInFilters filters, int time, int index, float lod, int w, int h)
{
	KX_2DFilter *bloom;

	RAS_2DFilterData bloomData;
	bloomData.filterMode = FILTER_MODE::FILTER_CUSTOMFILTER;
	bloomData.filterPassIndex = index;
	bloomData.gameObject = nullptr;
	bloomData.mipmap = false;
	bloomData.propertyNames = {};
	bloomData.buildInFilters = filters;
	
	if (time == 0)
		bloomData.shaderText = datatoc_RAS_Bloom2DFilter_buf_glsl;
	else if (time == 1)
		bloomData.shaderText = datatoc_RAS_Bloom2DFilter_bufH_glsl;
	else if (time == 2)
		bloomData.shaderText = datatoc_RAS_Bloom2DFilter_bufV_glsl;

	bloom = static_cast<KX_2DFilter *>(AddFilter(bloomData, true));
	KX_2DFilterOffScreen *offScreen = new KX_2DFilterOffScreen(1, (RAS_2DFilterOffScreen::Flag)0, (w / lod), (h / lod),
																   RAS_Rasterizer::HdrType::RAS_HDR_NONE);
	bloom->SetOffScreen(offScreen);

	return bloom;
}

RAS_2DFilter *KX_2DFilterManager::NewFilter(RAS_2DFilterData& filterData)
{
	return new KX_2DFilter(filterData);
}

#ifdef WITH_PYTHON
PyMethodDef KX_2DFilterManager::Methods[] = {
	// creation
	EXP_PYMETHODTABLE(KX_2DFilterManager, getFilter),
	EXP_PYMETHODTABLE(KX_2DFilterManager, addFilter),
	EXP_PYMETHODTABLE(KX_2DFilterManager, removeFilter),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeTonemapValues),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeBloomValues),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeLightScatterValues),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeSSRValues),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeSSAOValues),
	EXP_PYMETHODTABLE(KX_2DFilterManager, fxaa),
	EXP_PYMETHODTABLE(KX_2DFilterManager, changeVignetteValues),
	{nullptr, nullptr} //Sentinel
};

PyAttributeDef KX_2DFilterManager::Attributes[] = {
	EXP_PYATTRIBUTE_NULL //Sentinel
};

PyTypeObject KX_2DFilterManager::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"KX_2DFilterManager",
	sizeof(EXP_PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0, 0, 0, 0, 0, 0, 0,
	Methods,
	0,
	0,
	&EXP_PyObjectPlus::Type,
	0, 0, 0, 0, 0, 0,
	py_base_new
};


EXP_PYMETHODDEF_DOC(KX_2DFilterManager, getFilter, " getFilter(index)")
{
	int index = 0;

	if (!PyArg_ParseTuple(args, "i:getFilter", &index)) {
		return nullptr;
	}

	if (index < 0) {
		PyErr_SetString(PyExc_ValueError, "The index cannot be negative.");
		return nullptr;
	}

	KX_2DFilter *filter = (KX_2DFilter *)GetFilterPass(index, false);

	if (filter) {
		return filter->GetProxy();
	}

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, addFilter, " addFilter(index, type, fragmentProgram)")
{
	int index = 0;
	int type = 0;
	const char *frag = "";

	if (!PyArg_ParseTuple(args, "ii|s:addFilter", &index, &type, &frag)) {
		return nullptr;
	}

	if (index < 0) {
		PyErr_SetString(PyExc_ValueError, "The index cannot be negative.");
		return nullptr;
	}

	if (GetFilterPass(index, false)) {
		PyErr_Format(PyExc_ValueError, "filterManager.addFilter(index, type, fragmentProgram): KX_2DFilterManager, found existing filter in index (%i)", index);
		return nullptr;
	}

	if (type < FILTER_BLUR || type > FILTER_CUSTOMFILTER) {
		PyErr_SetString(PyExc_ValueError, "filterManager.addFilter(index, type, fragmentProgram): KX_2DFilterManager, type invalid");
		return nullptr;
	}

	if (strlen(frag) > 0 && type != FILTER_CUSTOMFILTER) {
		CM_PythonFunctionWarning("KX_2DFilterManager", "addFilter", "non-empty fragment program with non-custom filter type");
	}

	RAS_2DFilterData data;
	data.filterPassIndex = index;
	data.filterMode = type;
	data.shaderText = std::string(frag);

	KX_2DFilter *filter = static_cast<KX_2DFilter *>(AddFilter(data, false));

	return filter->GetProxy();
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, removeFilter, " removeFilter(index)")
{
	int index = 0;

	if (!PyArg_ParseTuple(args, "i:removeFilter", &index)) {
		return nullptr;
	}

	RemoveFilterPass(index);

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeTonemapValues, "changeTonemapValues(enabled, exposure, gamma, saturation, temperature)")
{
	int enabled;
	float exposure = -1;
	float gamma;
	float saturation;
	float temperature;

	if (!PyArg_ParseTuple(args, "i|ffff:changeTonemapValues", &enabled, &exposure, &gamma, &saturation, &temperature)) {
		return nullptr;
	}

	RAS_2DFilter *Tonemap = GetFilterPass(FILTERPASS_TONEMAP, true);

	if (!Tonemap)
		Py_RETURN_NONE;

	Tonemap->SetEnabled(enabled);

	if (exposure == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = Tonemap->GetBuildInFilters();
	filterParams->tonemap_exposure = exposure;
	filterParams->tonemap_gamma = gamma;
	filterParams->tonemap_saturation = saturation;
	filterParams->tonemap_temperature = temperature;

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeBloomValues, "changeBloomValues(enabled, intensity, threshold)")
{
	int enabled;
	float intensity = -1;
	float threshold;

	if (!PyArg_ParseTuple(args, "i|ff:changeBloomValues", &enabled, &intensity, &threshold)) {
		return nullptr;
	}

	RAS_2DFilter *Bloom0 = GetFilterPass(FILTERPASS_BLOOM_BUFFER, true);
	RAS_2DFilter *Bloom1 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 1, true);
	RAS_2DFilter *Bloom2 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 2, true);
	RAS_2DFilter *Bloom3 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 3, true);
	RAS_2DFilter *Bloom4 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 4, true);
	RAS_2DFilter *Bloom5 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 5, true);
	RAS_2DFilter *Bloom6 = GetFilterPass(FILTERPASS_BLOOM_BUFFER + 6, true);
	RAS_2DFilter *Bloom_Image = GetFilterPass(FILTERPASS_BLOOM_IMAGE, true);

	if (!Bloom_Image)
		Py_RETURN_NONE;

	Bloom0->SetEnabled(enabled);
	Bloom1->SetEnabled(enabled);
	Bloom2->SetEnabled(enabled);
	Bloom3->SetEnabled(enabled);
	Bloom4->SetEnabled(enabled);
	Bloom5->SetEnabled(enabled);
	Bloom_Image->SetEnabled(enabled);

	if (intensity == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = Bloom0->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom1->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom2->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom3->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom4->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom5->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom6->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	filterParams = Bloom_Image->GetBuildInFilters();
	filterParams->bloom_intensity = intensity;
	filterParams->bloom_threshold = threshold;

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeLightScatterValues, "changeLightScatterValues(enabled, step_max, step_size, threshold, intensity)")
{
	int enabled;
	float step_max = -1;
	float step_size, threshold, intensity;

	if (!PyArg_ParseTuple(args, "i|ffff:changeLightScatterValues", &enabled, &step_max, &step_size, &threshold, &intensity)) {
		return nullptr;
	}

	RAS_2DFilter *Scatter = GetFilterPass(FILTERPASS_LIGHTSCATTER, true);

	if (!Scatter)
		Py_RETURN_NONE;

	Scatter->SetEnabled(enabled);

	if (step_max == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = Scatter->GetBuildInFilters();
	filterParams->scatter_step_max = step_max;
	filterParams->scatter_step_size = step_size;
	filterParams->scatter_threshold = threshold;
	filterParams->scatter_intensity = intensity;

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeSSRValues, "changeSSRValues(enabled, step_max, bias, max_distance)")
{
	int enabled;
	float step_max = -1;
	float bias, max_distance;

	if (!PyArg_ParseTuple(args, "i|fff:changeSSRValues", &enabled, &step_max, &bias, &max_distance)) {
		return nullptr;
	}

	RAS_2DFilter *SSR_BLUR = GetFilterPass(FILTERPASS_SSR_BLUR, true);
	RAS_2DFilter *SSR_BUFFER = GetFilterPass(FILTERPASS_SSR_BUFFER, true);
	RAS_2DFilter *SSR_IMAGE = GetFilterPass(FILTERPASS_SSR_IMAGE, true);

	if (!SSR_BLUR)
		Py_RETURN_NONE;

	SSR_BLUR->SetEnabled(enabled);
	SSR_BUFFER->SetEnabled(enabled);
	SSR_IMAGE->SetEnabled(enabled);

	if (step_max == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = SSR_BLUR->GetBuildInFilters();
	filterParams->ssr_step_max = step_max;
	filterParams->ssr_bias = bias;
	filterParams->ssr_max_distance = max_distance;

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeSSAOValues, "changeSSAOValues(enabled, samples, strength, distance, attenuation, use_ssi=0, ssi_irradiance=0)")
{
	int enabled, use_ssi;
	float samples = -1;
	float strength, distance, attenuation, ssi_irradiance;

	if (!PyArg_ParseTuple(args, "i|ffffif:changeSSAOValues", &enabled, &samples, &strength, &distance, &attenuation, &use_ssi, &ssi_irradiance)) {
		return nullptr;
	}

	RAS_2DFilter *SSAO_BUFFER = GetFilterPass(FILTERPASS_SSAO_BUFFER, true);
	RAS_2DFilter *SSAO_BLUR = GetFilterPass(FILTERPASS_SSAO_BLUR, true);
	RAS_2DFilter *SSAO_IMAGE = GetFilterPass(FILTERPASS_SSAO_IMAGE, true);

	if (!SSAO_BLUR)
		Py_RETURN_NONE;

	SSAO_BLUR->SetEnabled(enabled);
	SSAO_BUFFER->SetEnabled(enabled);
	SSAO_IMAGE->SetEnabled(enabled);

	if (samples == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = SSAO_BUFFER->GetBuildInFilters();
	filterParams->ssao_samples = samples;
	filterParams->ssao_strength = strength;
	filterParams->ssao_distance = distance;
	filterParams->ssao_attenuation = attenuation;

	filterParams->use_SSAO_GI = use_ssi;
	filterParams->ssao_gi_irradiance = ssi_irradiance;

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, fxaa, "fxaa(enabled)")
{
	int enabled;

	if (!PyArg_ParseTuple(args, "i:fxaa", &enabled)) {
		return nullptr;
	}

	RAS_2DFilter *fxaa = GetFilterPass(FILTERPASS_FXAA, true);

	if (!fxaa)
		Py_RETURN_NONE;

	fxaa->SetEnabled(enabled);

	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC(KX_2DFilterManager, changeVignetteValues, "changeVignetteValues(enabled, size, radius)")
{
	int enabled;
	float size = -1;
	float radius;

	if (!PyArg_ParseTuple(args, "i|ff:changeVignetteValues", &enabled, &size, &radius)) {
		return nullptr;
	}

	RAS_2DFilter *Vignette = GetFilterPass(FILTERPASS_VIGNETTE, true);

	if (!Vignette)
		Py_RETURN_NONE;

	Vignette->SetEnabled(enabled);

	if (size == -1)
		Py_RETURN_NONE;

	BuildInFilters *filterParams = Vignette->GetBuildInFilters();
	filterParams->vignette_size = size;
	filterParams->vignette_radius = radius;

	Py_RETURN_NONE;
}

#endif  // WITH_PYTHON
