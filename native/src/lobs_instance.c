/*
 * LOBS plugin for OBS Studio.
 * Copyright (C) 2026 LOBS contributors. GPL-2.0-or-later.
 *
 * Built-in source "lobs_instance": renders another (shared) source, cropped.
 * LOBS creates one per layer, so every layer gets its own filter chain while the
 * underlying device is captured once.
 */
#include <obs.h>
#include <graphics/graphics.h>
#include <util/dstr.h>


struct lobs_instance {
	obs_source_t *self;
	obs_weak_source_t *child;
	char *child_name;
	int crop_l, crop_t, crop_r, crop_b;
	bool flip_h, flip_v;
	gs_texrender_t *texrender;
};

static const char *li_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "LOBS Instance";
}

static obs_source_t *li_get_child(struct lobs_instance *li)
{
	return li->child ? obs_weak_source_get_source(li->child) : NULL;
}

static void li_set_child(struct lobs_instance *li, const char *name)
{
	obs_weak_source_release(li->child);
	li->child = NULL;
	bfree(li->child_name);
	li->child_name = bstrdup(name);
	if (!name || !*name)
		return;
	obs_source_t *src = obs_get_source_by_name(name);
	if (src && src != li->self) {
		li->child = obs_source_get_weak_source(src);
	}
	obs_source_release(src);
}

static void li_update(void *data, obs_data_t *settings)
{
	struct lobs_instance *li = data;
	const char *name = obs_data_get_string(settings, "source");
	if (!li->child_name || strcmp(li->child_name, name) != 0)
		li_set_child(li, name);
	li->crop_l = (int)obs_data_get_int(settings, "crop_left");
	li->crop_t = (int)obs_data_get_int(settings, "crop_top");
	li->crop_r = (int)obs_data_get_int(settings, "crop_right");
	li->crop_b = (int)obs_data_get_int(settings, "crop_bottom");
	li->flip_h = obs_data_get_bool(settings, "flip_h");
	li->flip_v = obs_data_get_bool(settings, "flip_v");
}

static void *li_create(obs_data_t *settings, obs_source_t *source)
{
	struct lobs_instance *li = bzalloc(sizeof(*li));
	li->self = source;
	li_update(li, settings);
	return li;
}

static void li_destroy(void *data)
{
	struct lobs_instance *li = data;
	obs_weak_source_release(li->child);
	bfree(li->child_name);
	obs_enter_graphics();
	gs_texrender_destroy(li->texrender);
	obs_leave_graphics();
	bfree(li);
}

static void li_content_size(struct lobs_instance *li, obs_source_t *child, uint32_t *w, uint32_t *h)
{
	int cw = (int)obs_source_get_width(child), ch = (int)obs_source_get_height(child);
	int ow = cw - li->crop_l - li->crop_r, oh = ch - li->crop_t - li->crop_b;
	*w = ow > 0 ? (uint32_t)ow : 0;
	*h = oh > 0 ? (uint32_t)oh : 0;
}

static uint32_t li_get_width(void *data)
{
	struct lobs_instance *li = data;
	obs_source_t *child = li_get_child(li);
	uint32_t w = 0, h = 0;
	if (child)
		li_content_size(li, child, &w, &h);
	obs_source_release(child);
	return w;
}

static uint32_t li_get_height(void *data)
{
	struct lobs_instance *li = data;
	obs_source_t *child = li_get_child(li);
	uint32_t w = 0, h = 0;
	if (child)
		li_content_size(li, child, &w, &h);
	obs_source_release(child);
	return h;
}

static void li_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct lobs_instance *li = data;
	// The shared input may be created after this instance (e.g. on load); keep looking it up.
	if (!li->child && li->child_name && *li->child_name) {
		char *name = bstrdup(li->child_name);
		li_set_child(li, name);
		bfree(name);
	}
}

static void li_video_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct lobs_instance *li = data;
	obs_source_t *child = li_get_child(li);
	if (!child)
		return;

	uint32_t cw = obs_source_get_width(child), ch = obs_source_get_height(child);
	uint32_t w, h;
	li_content_size(li, child, &w, &h);
	if (!w || !h) {
		obs_source_release(child);
		return;
	}

	if (!li->texrender)
		li->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_texrender_reset(li->texrender);

	gs_blend_state_push();
	// Render the child premultiplied (color × alpha), the way the draw below expects it.
	// Plain ONE/ZERO kept straight alpha and gave text and other soft edges a harsh fringe.
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	if (gs_texrender_begin(li->texrender, cw, ch)) {
		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, (float)cw, 0.0f, (float)ch, -100.0f, 100.0f);
		obs_source_video_render(child);
		gs_texrender_end(li->texrender);
	}
	gs_blend_state_pop();

	gs_texture_t *tex = gs_texrender_get_texture(li->texrender);
	if (tex) {
		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), tex);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		// Mirroring happens here, inside the layer's own box, so position and size are unchanged.
		gs_matrix_push();
		if (li->flip_h) {
			gs_matrix_translate3f((float)w, 0.0f, 0.0f);
			gs_matrix_scale3f(-1.0f, 1.0f, 1.0f);
		}
		if (li->flip_v) {
			gs_matrix_translate3f(0.0f, (float)h, 0.0f);
			gs_matrix_scale3f(1.0f, -1.0f, 1.0f);
		}
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite_subregion(tex, 0, (uint32_t)li->crop_l, (uint32_t)li->crop_t, w, h);
		gs_matrix_pop();
		gs_blend_state_pop();
	}
	obs_source_release(child);
}

static void li_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct lobs_instance *li = data;
	obs_source_t *child = li_get_child(li);
	if (child) {
		cb(li->self, child, param);
		obs_source_release(child);
	}
}

static void li_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "source", "");
}

static obs_properties_t *li_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "source", "Source (managed by LOBS)", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "crop_left", "Crop left", 0, 16384, 1);
	obs_properties_add_int(props, "crop_top", "Crop top", 0, 16384, 1);
	obs_properties_add_int(props, "crop_right", "Crop right", 0, 16384, 1);
	obs_properties_add_int(props, "crop_bottom", "Crop bottom", 0, 16384, 1);
	return props;
}

static struct obs_source_info lobs_instance_info = {
	.id = "lobs_instance",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE,
	.icon_type = OBS_ICON_TYPE_CUSTOM,
	.get_name = li_get_name,
	.create = li_create,
	.destroy = li_destroy,
	.update = li_update,
	.get_width = li_get_width,
	.get_height = li_get_height,
	.get_defaults = li_defaults,
	.get_properties = li_properties,
	.video_tick = li_video_tick,
	.video_render = li_video_render,
	.enum_active_sources = li_enum_sources,
};

void lobs_register_instance_source(void)
{
	obs_register_source(&lobs_instance_info);
}
