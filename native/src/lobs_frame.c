/*
 * Built-in source "lobs_frame": shows the latest image LOBS hands it (straight-alpha BGRA),
 * uploaded to the GPU on the render thread. LOBS draws shapes and the laser pointer with
 * QPainter and pushes the pixels here, so vector graphics stay crisp at the layer's size.
 */
#include <obs-module.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <util/bmem.h>
#include <string.h>

struct lobs_frame {
	obs_source_t *source;
	SRWLOCK lock;
	uint8_t *pending;   // new pixels waiting for upload (owned)
	uint32_t pw, ph;
	gs_texture_t *tex;
	uint32_t w, h;      // size of what's shown
};

static const char *lf_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "LOBS frame";
}

static void *lf_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct lobs_frame *f = bzalloc(sizeof(*f));
	f->source = source;
	InitializeSRWLock(&f->lock);
	return f;
}

static void lf_destroy(void *data)
{
	struct lobs_frame *f = data;
	obs_enter_graphics();
	gs_texture_destroy(f->tex);
	obs_leave_graphics();
	bfree(f->pending);
	bfree(f);
}

static uint32_t lf_width(void *data) { return ((struct lobs_frame *)data)->w; }
static uint32_t lf_height(void *data) { return ((struct lobs_frame *)data)->h; }

static void lf_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct lobs_frame *f = data;
	AcquireSRWLockExclusive(&f->lock);
	if (f->pending) {
		if (!f->tex || f->pw != gs_texture_get_width(f->tex) || f->ph != gs_texture_get_height(f->tex)) {
			gs_texture_destroy(f->tex);
			const uint8_t *planes[] = {f->pending};
			f->tex = gs_texture_create(f->pw, f->ph, GS_BGRA, 1, planes, GS_DYNAMIC);
		} else {
			gs_texture_set_image(f->tex, f->pending, f->pw * 4, false);
		}
		f->w = f->pw;
		f->h = f->ph;
		bfree(f->pending);
		f->pending = NULL;
	}
	ReleaseSRWLockExclusive(&f->lock);
	if (f->tex && f->w && f->h)
		obs_source_draw(f->tex, 0, 0, 0, 0, false);
}

static struct obs_source_info lobs_frame_info = {
	.id = "lobs_frame",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = lf_get_name,
	.create = lf_create,
	.destroy = lf_destroy,
	.get_width = lf_width,
	.get_height = lf_height,
	.video_render = lf_render,
};

void lobs_register_frame_source(void)
{
	obs_register_source(&lobs_frame_info);
}

/* Hands the source a new image (copied). Width/height of 0 clears it. */
void lobs_frame_set(obs_source_t *source, const uint8_t *bgra, uint32_t width, uint32_t height, uint32_t stride)
{
	if (!source)
		return;
	struct lobs_frame *f = obs_obj_get_data(source);
	if (!f)
		return;
	uint8_t *copy = NULL;
	if (width && height) {
		copy = bmalloc((size_t)width * height * 4);
		for (uint32_t y = 0; y < height; y++)
			memcpy(copy + (size_t)y * width * 4, bgra + (size_t)y * stride, (size_t)width * 4);
	}
	AcquireSRWLockExclusive(&f->lock);
	bfree(f->pending);
	f->pending = copy;
	f->pw = width;
	f->ph = height;
	// Report the new size right away: a source with no size is never drawn, so waiting for the
	// upload (which happens while drawing) would never show anything.
	f->w = copy ? width : 0;
	f->h = copy ? height : 0;
	ReleaseSRWLockExclusive(&f->lock);
}
