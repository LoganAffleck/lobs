/*
 * Built-in source "lobs_frame": shows the latest image LOBS hands it (straight-alpha BGRA),
 * uploaded to the GPU on the render thread. LOBS draws shapes and the laser pointer with
 * QPainter and pushes the pixels here, so vector graphics stay crisp at the layer's size.
 */
#include <obs-module.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef SRWLOCK frame_lock_t;
#define frame_lock_init(l) InitializeSRWLock(l)
#define frame_lock(l) AcquireSRWLockExclusive(l)
#define frame_unlock(l) ReleaseSRWLockExclusive(l)
#define frame_lock_free(l)
#else
#include <pthread.h>
typedef pthread_mutex_t frame_lock_t;
#define frame_lock_init(l) pthread_mutex_init(l, NULL)
#define frame_lock(l) pthread_mutex_lock(l)
#define frame_unlock(l) pthread_mutex_unlock(l)
#define frame_lock_free(l) pthread_mutex_destroy(l)
#endif
#include <util/bmem.h>
#include <string.h>

struct lobs_frame {
	obs_source_t *source;
	frame_lock_t lock;
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
	frame_lock_init(&f->lock);
	return f;
}

static void lf_destroy(void *data)
{
	struct lobs_frame *f = data;
	obs_enter_graphics();
	gs_texture_destroy(f->tex);
	obs_leave_graphics();
	bfree(f->pending);
	frame_lock_free(&f->lock);
	bfree(f);
}

static uint32_t lf_width(void *data) { return ((struct lobs_frame *)data)->w; }
static uint32_t lf_height(void *data) { return ((struct lobs_frame *)data)->h; }

static void lf_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct lobs_frame *f = data;
	frame_lock(&f->lock);
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
	frame_unlock(&f->lock);
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
	frame_lock(&f->lock);
	bfree(f->pending);
	f->pending = copy;
	f->pw = width;
	f->ph = height;
	// Report the new size right away: a source with no size is never drawn, so waiting for the
	// upload (which happens while drawing) would never show anything.
	f->w = copy ? width : 0;
	f->h = copy ? height : 0;
	frame_unlock(&f->lock);
}
