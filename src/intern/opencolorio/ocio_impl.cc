/*
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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

#include <iostream>
#include <sstream>
#include <string.h>
#include <memory>

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4251 4275)
#endif
#include <OpenColorIO/OpenColorIO.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

#if !defined(WITH_ASSERT_ABORT)
#  define OCIO_abort()
#else
#  include <stdlib.h>
#  define OCIO_abort() abort()
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

/* ============================================================
 * OCIO version compatibility
 * ============================================================ */
/* OCIO 2.3+: getDefaultCPUProcessor() replaces direct apply() on Processor */
#if OCIO_VERSION_HEX >= 0x02030000
#  define OCIO_PROCESSOR_APPLY(p, img)    (p)->getDefaultCPUProcessor()->apply(*(PackedImageDesc *)(img))
#  define OCIO_PROCESSOR_APPLY_RGB(p, px) (p)->getDefaultCPUProcessor()->applyRGB(px)
#  define OCIO_PROCESSOR_APPLY_RGBA(p, px)(p)->getDefaultCPUProcessor()->applyRGBA(px)
#  define OCIO_GET_DISPLAY_COLORSPACE(cfg, display, view) _ociov23_getDisplayColorSpaceName(cfg, display, view)
#  define OCIO_PACKED_IMAGE_DESC_ARGS 6
/* DisplayTransformRcPtr typedef was removed in OCIO 2.3 public headers.
 * Use std::shared_ptr<const DisplayTransform> as a compatible replacement. */
typedef std::shared_ptr<const DisplayTransform> DisplayTransformRcPtrAlias;
#  define DISPLAY_TRANSFORM_RCPTR DisplayTransformRcPtrAlias
#else
#  define OCIO_PROCESSOR_APPLY(p, img)    (p)->apply(*(PackedImageDesc *)(img))
#  define OCIO_PROCESSOR_APPLY_RGB(p, px) (p)->applyRGB(px)
#  define OCIO_PROCESSOR_APPLY_RGBA(p, px)(p)->applyRGBA(px)
#  define OCIO_GET_DISPLAY_COLORSPACE(cfg, display, view) _ociov23_getDisplayColorSpaceName(cfg, display, view)
#  define OCIO_PACKED_IMAGE_DESC_ARGS 7
#  define DISPLAY_TRANSFORM_RCPTR DisplayTransformRcPtr
#endif

/* NOTe: This is because OCIO 1.1.0 has a bug which makes default
 * display to be the one which is first alphabetically.
 *
 * Fix has been submitted as a patch
 *   https://github.com/imageworks/OpenColorIO/pull/638
 *
 * For until then we use first usable display instead. */
#define DEFAULT_DISPLAY_WORKAROUND
#ifdef DEFAULT_DISPLAY_WORKAROUND
#  include <mutex>
#endif

static void OCIO_reportError(const char *err)
{
	std::cerr << "OpenColorIO Error: " << err << std::endl;

	OCIO_abort();
}

static void OCIO_reportException(Exception &exception)
{
	OCIO_reportError(exception.what());
}

#if OCIO_VERSION_HEX >= 0x02030000
/* In OCIO 2.3, getDisplayColorSpaceName takes (display, viewIndex) not (display, view).
 * Find index by view name. */
static const char *_ociov23_getDisplayColorSpaceName(ConstConfigRcPtr cfg, const char *display, const char *view)
{
	int numViews = cfg->getNumViews(display);
	for (int i = 0; i < numViews; i++) {
		if (strcmp(cfg->getView(display, i), view) == 0) {
			return cfg->getDisplayColorSpaceName(display, i);
		}
	}
	return cfg->getDisplayColorSpaceName(display, 0);
}
#endif

OCIO_ConstConfigRcPtr *OCIOImpl::getCurrentConfig(void)
{
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = GetCurrentConfig();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIOImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr *config)
{
	try {
		SetCurrentConfig(*(ConstConfigRcPtr *) config);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromEnv(void)
{
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromEnv();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

	return NULL;
}


OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromFile(const char *filename)
{
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromFile(filename);

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIOImpl::configRelease(OCIO_ConstConfigRcPtr *config)
{
	OBJECT_GUARDED_DELETE((ConstConfigRcPtr *) config, ConstConfigRcPtr);
}

int OCIOImpl::configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumColorSpaces();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getColorSpaceNameByIndex(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

OCIO_ConstColorSpaceRcPtr *OCIOImpl::configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
	ConstColorSpaceRcPtr *cs = OBJECT_GUARDED_NEW(ConstColorSpaceRcPtr);

	try {
		*cs = (*(ConstConfigRcPtr *) config)->getColorSpace(name);

		if (*cs)
			return (OCIO_ConstColorSpaceRcPtr *) cs;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(cs, ConstColorSpaceRcPtr);

	return NULL;
}

int OCIOImpl::configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getIndexForColorSpace(name);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return -1;
}

const char *OCIOImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config)
{
#ifdef DEFAULT_DISPLAY_WORKAROUND
	if (getenv("OCIO_ACTIVE_DISPLAYS") == NULL) {
		const char *active_displays =
		        (*(ConstConfigRcPtr *) config)->getActiveDisplays();
		const char *separator_pos = strchr(active_displays, ',');
		if (separator_pos == NULL) {
			return active_displays;
		}
		static std::string active_display;
		/* NOTE: Configuration is shared and is never changed during runtime,
		 * so we only guarantee two threads don't initialize at the same. */
		static std::mutex mutex;
		mutex.lock();
		if (active_display.empty()) {
			active_display = active_displays;
			active_display[separator_pos - active_displays] = '\0';
		}
		mutex.unlock();
		return active_display.c_str();
	}
#endif

	try {
		return (*(ConstConfigRcPtr *) config)->getDefaultDisplay();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIOImpl::configGetNumDisplays(OCIO_ConstConfigRcPtr* config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumDisplays();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetDisplay(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getDisplay(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIOImpl::configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display)
{
#ifdef DEFAULT_DISPLAY_WORKAROUND
	/* NOTE: We assume that first active view always exists for a default
	 * display. */
	if (getenv("OCIO_ACTIVE_VIEWS") == NULL) {
		const char *active_views =
		        (*(ConstConfigRcPtr *) config)->getActiveViews();
		const char *separator_pos = strchr(active_views, ',');
		if (separator_pos == NULL) {
			return active_views;
		}
		static std::string active_view;
		/* NOTE: Configuration is shared and is never changed during runtime,
		 * so we only guarantee two threads don't initialize at the same. */
		static std::mutex mutex;
		mutex.lock();
		if (active_view.empty()) {
			active_view = active_views;
			active_view[separator_pos - active_views] = '\0';
		}
		mutex.unlock();
		return active_view.c_str();
	}
#endif
	try {
		return (*(ConstConfigRcPtr *) config)->getDefaultView(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIOImpl::configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumViews(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getView(display, index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIOImpl::configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view)
{
	try {
		ConstConfigRcPtr cfg = *(ConstConfigRcPtr *)config;
		return OCIO_GET_DISPLAY_COLORSPACE(cfg, display, view);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIOImpl::configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb)
{
	try {
		double luma[3];
		(*(ConstConfigRcPtr *) config)->getDefaultLumaCoefs(luma);
		rgb[0] = (float)luma[0]; rgb[1] = (float)luma[1]; rgb[2] = (float)luma[2];
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

int OCIOImpl::configGetNumLooks(OCIO_ConstConfigRcPtr *config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumLooks();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getLookNameByIndex(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

OCIO_ConstLookRcPtr *OCIOImpl::configGetLook(OCIO_ConstConfigRcPtr *config, const char *name)
{
	ConstLookRcPtr *look = OBJECT_GUARDED_NEW(ConstLookRcPtr);

	try {
		*look = (*(ConstConfigRcPtr *) config)->getLook(name);

		if (*look)
			return (OCIO_ConstLookRcPtr *) look;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(look, ConstLookRcPtr);

	return NULL;
}

const char *OCIOImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
	return (*(ConstLookRcPtr *) look)->getProcessSpace();
}

void OCIOImpl::lookRelease(OCIO_ConstLookRcPtr *look)
{
	OBJECT_GUARDED_DELETE((ConstLookRcPtr *) look, ConstLookRcPtr);
}

int OCIOImpl::colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs_)
{
	ConstColorSpaceRcPtr *cs = (ConstColorSpaceRcPtr *) cs_;
	const char *family = (*cs)->getFamily();

	if (!strcmp(family, "rrt") || !strcmp(family, "display")) {
		/* assume display and rrt transformations are not invertible
		 * in fact some of them could be, but it doesn't make much sense to allow use them as invertible
		 */
		return false;
	}

	if ((*cs)->isData()) {
		/* data color spaces don't have transformation at all */
		return true;
	}

	if ((*cs)->getTransform(COLORSPACE_DIR_TO_REFERENCE)) {
		/* if there's defined transform to reference space, color space could be converted to scene linear */
		return true;
	}

	return true;
}

int OCIOImpl::colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->isData();
}

void OCIOImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs)
{
	OBJECT_GUARDED_DELETE((ConstColorSpaceRcPtr *) cs, ConstColorSpaceRcPtr);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(srcName, dstName);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);

	return 0;
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform)
{
	ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(*(ConstTransformRcPtr *) transform);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);

	return NULL;
}

void OCIOImpl::processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
	try {
		ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
		OCIO_PROCESSOR_APPLY(p, img);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIOImpl::processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img_)
{
	try {
		PackedImageDesc *img = (PackedImageDesc *) img_;
		int channels = img->getNumChannels();

		if (channels == 4) {
			float *pixels = (float *)img->getData();

			int width = img->getWidth();
			int height = img->getHeight();

			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float *pixel = pixels + 4 * (y * width + x);

					processorApplyRGBA_predivide(processor, pixel);
				}
			}
		}
		else {
			ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
			OCIO_PROCESSOR_APPLY(p, img);
		}
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIOImpl::processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
	OCIO_PROCESSOR_APPLY_RGB(p, pixel);
}

void OCIOImpl::processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
	OCIO_PROCESSOR_APPLY_RGBA(p, pixel);
}

void OCIOImpl::processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
		ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
		OCIO_PROCESSOR_APPLY_RGBA(p, pixel);
	}
	else {
		float alpha, inv_alpha;

		alpha = pixel[3];
		inv_alpha = 1.0f / alpha;

		pixel[0] *= inv_alpha;
		pixel[1] *= inv_alpha;
		pixel[2] *= inv_alpha;

		ConstProcessorRcPtr p = *(ConstProcessorRcPtr *) processor;
		OCIO_PROCESSOR_APPLY_RGBA(p, pixel);

		pixel[0] *= alpha;
		pixel[1] *= alpha;
		pixel[2] *= alpha;
	}
}

void OCIOImpl::processorRelease(OCIO_ConstProcessorRcPtr *p)
{
	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);
}

const char *OCIOImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->getName();
}

const char *OCIOImpl::colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->getDescription();
}

const char *OCIOImpl::colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *)cs)->getFamily();
}

OCIO_DisplayTransformRcPtr *OCIOImpl::createDisplayTransform(void)
{
	DISPLAY_TRANSFORM_RCPTR *dt = OBJECT_GUARDED_NEW(DISPLAY_TRANSFORM_RCPTR);

	*dt = DisplayTransform::Create();

	return (OCIO_DisplayTransformRcPtr *) dt;
}

void OCIOImpl::displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setInputColorSpaceName(name);
}

void OCIOImpl::displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setDisplay(name);
}

void OCIOImpl::displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setView(name);
}

void OCIOImpl::displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setDisplayCC(* (ConstTransformRcPtr *) t);
}

void OCIOImpl::displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setLinearCC(*(ConstTransformRcPtr *) t);
}

void OCIOImpl::displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setLooksOverride(looks);
}

void OCIOImpl::displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled)
{
	(*(DISPLAY_TRANSFORM_RCPTR *) dt)->setLooksOverrideEnabled(enabled);
}

void OCIOImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr *dt)
{
	OBJECT_GUARDED_DELETE((DISPLAY_TRANSFORM_RCPTR *) dt, DISPLAY_TRANSFORM_RCPTR);
}

OCIO_PackedImageDesc *OCIOImpl::createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
                                                           long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	try {
		void *mem = (void *)MEM_mallocN(sizeof(PackedImageDesc), __func__);
#if OCIO_PACKED_IMAGE_DESC_ARGS == 7
		PackedImageDesc *id = new(mem) PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);
#else
		/* OCIO 2.3: yStrideBytes removed from PackedImageDesc constructor */
		PackedImageDesc *id = new(mem) PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes);
#endif
		return (OCIO_PackedImageDesc *) id;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIOImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc* id)
{
	OBJECT_GUARDED_DELETE((PackedImageDesc *) id, PackedImageDesc);
}

OCIO_ExponentTransformRcPtr *OCIOImpl::createExponentTransform(void)
{
	ExponentTransformRcPtr *et = OBJECT_GUARDED_NEW(ExponentTransformRcPtr);

	*et = ExponentTransform::Create();

	return (OCIO_ExponentTransformRcPtr *) et;
}

void OCIOImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent)
{
	double tmp[4] = { exponent[0], exponent[1], exponent[2], exponent[3] };
	(*(ExponentTransformRcPtr *) et)->setValue(tmp);
}

void OCIOImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr *et)
{
	OBJECT_GUARDED_DELETE((ExponentTransformRcPtr *) et, ExponentTransformRcPtr);
}

OCIO_MatrixTransformRcPtr *OCIOImpl::createMatrixTransform(void)
{
	MatrixTransformRcPtr *mt = OBJECT_GUARDED_NEW(MatrixTransformRcPtr);

	*mt = MatrixTransform::Create();

	return (OCIO_MatrixTransformRcPtr *) mt;
}

void OCIOImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
#if OCIO_VERSION_HEX >= 0x02030000
	/* OCIO 2.3+ removed MatrixTransform::setValue(). Use MatrixTransform::setMatrix() and setOffset() instead. */
	double m44d[16], offd[4];
	for (int i = 0; i < 16; i++) m44d[i] = (double)m44[i];
	for (int i = 0; i < 4; i++) offd[i] = (double)offset4[i];
	(*(MatrixTransformRcPtr *) mt)->setMatrix(m44d);
	(*(MatrixTransformRcPtr *) mt)->setOffset(offd);
#else
	(*(MatrixTransformRcPtr *) mt)->setValue(m44, offset4);
#endif
}

void OCIOImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt)
{
	OBJECT_GUARDED_DELETE((MatrixTransformRcPtr *) mt, MatrixTransformRcPtr);
}

void OCIOImpl::matrixTransformScale(float *m44, float *offset4, const float *scale4f)
{
	double m44d[16], offd[4], scaled[4];
	for (int i = 0; i < 16; i++) m44d[i] = m44[i];
	for (int i = 0; i < 4; i++) { offd[i] = offset4[i]; scaled[i] = scale4f[i]; }
	MatrixTransform::Scale(m44d, offd, scaled);
	for (int i = 0; i < 16; i++) m44[i] = (float)m44d[i];
	for (int i = 0; i < 4; i++) offset4[i] = (float)offd[i];
}

const char *OCIOImpl::getVersionString(void)
{
	return GetVersion();
}

int OCIOImpl::getVersionHex(void)
{
	return GetVersionHex();
}
