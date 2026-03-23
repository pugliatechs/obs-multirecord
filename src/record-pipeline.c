#include "record-pipeline.h"

#include <obs-module.h>
#include <media-io/video-frame.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <time.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Config helpers                                                     */
/* ------------------------------------------------------------------ */

void record_pipeline_config_init(struct record_pipeline_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->container = bstrdup("mkv");
	cfg->video_encoder_id = bstrdup("obs_x264");
	cfg->audio_encoder_id = bstrdup("ffmpeg_aac");
	cfg->video_bitrate = 2500;
	cfg->audio_bitrate = 160;
	cfg->fps_num = 30;
	cfg->fps_den = 1;
	cfg->filename_format = bstrdup("%S_%Y%m%d_%H%M%S");
}

static char *safe_strdup(const char *s)
{
	return s ? bstrdup(s) : NULL;
}

void record_pipeline_config_copy(struct record_pipeline_config *dst,
				 const struct record_pipeline_config *src)
{
	dst->video_source_name = safe_strdup(src->video_source_name);
	dst->audio_source_name = safe_strdup(src->audio_source_name);
	dst->output_dir = safe_strdup(src->output_dir);
	dst->filename_format = safe_strdup(src->filename_format);
	dst->container = safe_strdup(src->container);
	dst->video_encoder_id = safe_strdup(src->video_encoder_id);
	dst->audio_encoder_id = safe_strdup(src->audio_encoder_id);
	dst->video_bitrate = src->video_bitrate;
	dst->audio_bitrate = src->audio_bitrate;
	dst->width = src->width;
	dst->height = src->height;
	dst->fps_num = src->fps_num;
	dst->fps_den = src->fps_den;
}

void record_pipeline_config_free(struct record_pipeline_config *cfg)
{
	bfree(cfg->video_source_name);
	bfree(cfg->audio_source_name);
	bfree(cfg->output_dir);
	bfree(cfg->filename_format);
	bfree(cfg->container);
	bfree(cfg->video_encoder_id);
	bfree(cfg->audio_encoder_id);
	memset(cfg, 0, sizeof(*cfg));
}

/* ------------------------------------------------------------------ */
/* Path builder                                                       */
/* ------------------------------------------------------------------ */

char *record_pipeline_build_path(const struct record_pipeline_config *config,
				 const char *source_name)
{
	struct dstr path = {0};
	time_t now = time(NULL);
	struct tm tm_info;
	localtime_r(&now, &tm_info);

	const char *fmt = config->filename_format;
	if (!fmt || !*fmt)
		fmt = "%S_%Y%m%d_%H%M%S";

	struct dstr filename = {0};
	for (const char *p = fmt; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			p++;
			switch (*p) {
			case 'S':
				/* Source name (sanitised) */
				if (source_name) {
					for (const char *s = source_name; *s;
					     s++) {
						char c = *s;
						if (c == '/' || c == '\\' ||
						    c == ':' || c == '*' ||
						    c == '?' || c == '"' ||
						    c == '<' || c == '>' ||
						    c == '|')
							c = '_';
						dstr_cat_ch(&filename, c);
					}
				} else {
					dstr_cat(&filename, "unknown");
				}
				break;
			case 'Y':
				dstr_catf(&filename, "%04d",
					  tm_info.tm_year + 1900);
				break;
			case 'm':
				dstr_catf(&filename, "%02d",
					  tm_info.tm_mon + 1);
				break;
			case 'd':
				dstr_catf(&filename, "%02d", tm_info.tm_mday);
				break;
			case 'H':
				dstr_catf(&filename, "%02d", tm_info.tm_hour);
				break;
			case 'M':
				dstr_catf(&filename, "%02d", tm_info.tm_min);
				break;
			case '%':
				dstr_cat_ch(&filename, '%');
				break;
			default:
				dstr_cat_ch(&filename, '%');
				dstr_cat_ch(&filename, *p);
				break;
			}
		} else {
			dstr_cat_ch(&filename, *p);
		}
	}

	const char *dir = config->output_dir;
	if (!dir || !*dir)
		dir = ".";

	const char *ext = config->container;
	if (!ext || !*ext)
		ext = "mkv";

	dstr_printf(&path, "%s/%s.%s", dir, filename.array, ext);

	dstr_free(&filename);
	return path.array; /* caller bfree()s */
}

/* ------------------------------------------------------------------ */
/* Encoder enumeration                                                */
/* ------------------------------------------------------------------ */

size_t record_pipeline_enum_video_encoders(
	bool (*cb)(void *param, const char *id, const char *name), void *param)
{
	size_t count = 0;
	const char *id = NULL;

	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;

		const char *name = obs_encoder_get_display_name(id);
		if (!name)
			name = id;

		count++;
		if (cb && !cb(param, id, name))
			break;
	}
	return count;
}

size_t record_pipeline_enum_audio_encoders(
	bool (*cb)(void *param, const char *id, const char *name), void *param)
{
	size_t count = 0;
	const char *id = NULL;

	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (obs_get_encoder_type(id) != OBS_ENCODER_AUDIO)
			continue;

		const char *name = obs_encoder_get_display_name(id);
		if (!name)
			name = id;

		count++;
		if (cb && !cb(param, id, name))
			break;
	}
	return count;
}

/* ------------------------------------------------------------------ */
/* Output signal handlers                                             */
/* ------------------------------------------------------------------ */

static void on_output_stop(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	struct record_pipeline *p = param;

	pthread_mutex_lock(&p->mutex);
	if (p->state == PIPELINE_RECORDING || p->state == PIPELINE_STOPPING) {
		p->state = PIPELINE_IDLE;
	}
	pthread_mutex_unlock(&p->mutex);

	if (p->state_callback)
		p->state_callback(p, p->state_callback_param);
}

static void on_output_start(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	struct record_pipeline *p = param;

	pthread_mutex_lock(&p->mutex);
	p->state = PIPELINE_RECORDING;
	pthread_mutex_unlock(&p->mutex);

	if (p->state_callback)
		p->state_callback(p, p->state_callback_param);
}

/* ------------------------------------------------------------------ */
/* Audio input callback                                               */
/* ------------------------------------------------------------------ */

static bool audio_input_callback(void *param, uint64_t start_ts,
				 uint64_t end_ts, uint64_t *out_ts,
				 uint32_t mixers,
				 struct audio_output_data *mixes)
{
	UNUSED_PARAMETER(mixers);
	struct record_pipeline *p = param;

	if (!p->audio_source)
		return false;

	/*
	 * Pull the source's current audio mix.  We request mix index 0.
	 * obs_source_get_audio_mix() fills `mixes[0].data` planes.
	 */
	struct obs_source_audio_mix source_mix;
	obs_source_get_audio_mix(p->audio_source, &source_mix);

	const struct audio_output_info *info =
		audio_output_get_info(p->audio_output);
	size_t channels = get_audio_channels(info->speakers);
	size_t frames = AUDIO_OUTPUT_FRAMES; /* per-block size */

	for (size_t ch = 0; ch < channels && ch < MAX_AV_PLANES; ch++) {
		if (source_mix.output[0].data[ch]) {
			memcpy(mixes[0].data[ch],
			       source_mix.output[0].data[ch],
			       frames * sizeof(float));
		}
	}

	*out_ts = start_ts;
	UNUSED_PARAMETER(end_ts);
	return true;
}

/* ------------------------------------------------------------------ */
/* Video rendering (tick-based capture into video_output)              */
/* ------------------------------------------------------------------ */

static void render_source_to_video_output(struct record_pipeline *p)
{
	if (!p->video_source || !p->video_output || !p->texrender ||
	    !p->stagesurface)
		return;

	uint32_t source_cx = obs_source_get_width(p->video_source);
	uint32_t source_cy = obs_source_get_height(p->video_source);
	if (source_cx == 0 || source_cy == 0)
		return;

	uint32_t cx = p->cx;
	uint32_t cy = p->cy;
	if (cx == 0 || cy == 0) {
		cx = source_cx;
		cy = source_cy;
	}

	/* Render to texture */
	gs_texrender_reset(p->texrender);
	if (!gs_texrender_begin(p->texrender, cx, cy))
		return;

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)source_cx, 0.0f, (float)source_cy, -100.0f,
		 100.0f);

	obs_source_video_render(p->video_source);
	gs_texrender_end(p->texrender);

	/* Stage to CPU */
	gs_texture_t *tex = gs_texrender_get_texture(p->texrender);
	if (!tex)
		return;

	gs_stage_texture(p->stagesurface, tex);

	uint8_t *data;
	uint32_t linesize;
	if (!gs_stagesurface_map(p->stagesurface, &data, &linesize))
		return;

	/* Copy into video_output frame */
	struct video_frame output_frame;
	if (video_output_lock_frame(p->video_output, &output_frame, 1,
				    os_gettime_ns())) {
		const struct video_output_info *voi =
			video_output_get_info(p->video_output);

		if (voi->format == VIDEO_FORMAT_NV12) {
			/* For NV12: just copy the raw staged data.
			 * The stagesurface format should match. */
			uint32_t h = cy;
			for (uint32_t y = 0; y < h; y++) {
				memcpy(output_frame.data[0] +
					       y * output_frame.linesize[0],
				       data + y * linesize,
				       cx < linesize ? cx : linesize);
			}
		} else {
			/* BGRA or other: single plane copy */
			uint32_t copy_linesize = cx * 4;
			if (copy_linesize > linesize)
				copy_linesize = linesize;
			if (copy_linesize > output_frame.linesize[0])
				copy_linesize = output_frame.linesize[0];
			for (uint32_t y = 0; y < cy; y++) {
				memcpy(output_frame.data[0] +
					       y * output_frame.linesize[0],
				       data + y * linesize, copy_linesize);
			}
		}

		video_output_unlock_frame(p->video_output);
	}

	gs_stagesurface_unmap(p->stagesurface);
}

/* Called on the graphics thread via obs_add_tick_callback */
static void pipeline_video_tick(void *param, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct record_pipeline *p = param;

	if (p->state != PIPELINE_RECORDING)
		return;

	render_source_to_video_output(p);
}

/* ------------------------------------------------------------------ */
/* Pipeline lifecycle                                                 */
/* ------------------------------------------------------------------ */

struct record_pipeline *
record_pipeline_create(const struct record_pipeline_config *config)
{
	struct record_pipeline *p = bzalloc(sizeof(*p));
	record_pipeline_config_copy(&p->config, config);
	pthread_mutex_init(&p->mutex, NULL);
	p->state = PIPELINE_IDLE;
	return p;
}

void record_pipeline_destroy(struct record_pipeline *pipeline)
{
	if (!pipeline)
		return;

	if (pipeline->state == PIPELINE_RECORDING ||
	    pipeline->state == PIPELINE_STARTING)
		record_pipeline_stop(pipeline);

	record_pipeline_config_free(&pipeline->config);
	pthread_mutex_destroy(&pipeline->mutex);
	bfree(pipeline->last_error);
	bfree(pipeline);
}

static void set_error(struct record_pipeline *p, const char *msg)
{
	bfree(p->last_error);
	p->last_error = bstrdup(msg);
	p->state = PIPELINE_ERROR;

	if (p->state_callback)
		p->state_callback(p, p->state_callback_param);
}

static void release_pipeline_objects(struct record_pipeline *p)
{
	if (p->tick_registered) {
		obs_remove_tick_callback(pipeline_video_tick, p);
		p->tick_registered = false;
	}

	if (p->file_output) {
		signal_handler_t *sh =
			obs_output_get_signal_handler(p->file_output);
		signal_handler_disconnect(sh, "start", on_output_start, p);
		signal_handler_disconnect(sh, "stop", on_output_stop, p);
		obs_output_release(p->file_output);
		p->file_output = NULL;
	}

	if (p->video_encoder) {
		obs_encoder_release(p->video_encoder);
		p->video_encoder = NULL;
	}
	if (p->audio_encoder) {
		obs_encoder_release(p->audio_encoder);
		p->audio_encoder = NULL;
	}

	obs_enter_graphics();
	if (p->stagesurface) {
		gs_stagesurface_destroy(p->stagesurface);
		p->stagesurface = NULL;
	}
	if (p->texrender) {
		gs_texrender_destroy(p->texrender);
		p->texrender = NULL;
	}
	obs_leave_graphics();

	if (p->video_output) {
		video_output_stop(p->video_output);
		video_output_close(p->video_output);
		p->video_output = NULL;
	}
	if (p->audio_output) {
		audio_output_close(p->audio_output);
		p->audio_output = NULL;
	}

	if (p->video_source) {
		obs_source_release(p->video_source);
		p->video_source = NULL;
	}
	if (p->audio_source) {
		obs_source_release(p->audio_source);
		p->audio_source = NULL;
	}
}

bool record_pipeline_start(struct record_pipeline *pipeline)
{
	struct record_pipeline *p = pipeline;
	if (!p)
		return false;

	pthread_mutex_lock(&p->mutex);
	if (p->state == PIPELINE_RECORDING || p->state == PIPELINE_STARTING) {
		pthread_mutex_unlock(&p->mutex);
		return false;
	}
	p->state = PIPELINE_STARTING;
	pthread_mutex_unlock(&p->mutex);

	/* 1. Resolve sources */
	if (!p->config.video_source_name || !*p->config.video_source_name) {
		set_error(p, "No video source configured");
		return false;
	}

	p->video_source =
		obs_get_source_by_name(p->config.video_source_name);
	if (!p->video_source) {
		set_error(p, "Video source not found");
		return false;
	}

	/* Audio source: explicit or same as video */
	if (p->config.audio_source_name && *p->config.audio_source_name) {
		p->audio_source =
			obs_get_source_by_name(p->config.audio_source_name);
		if (!p->audio_source) {
			set_error(p, "Audio source not found");
			release_pipeline_objects(p);
			return false;
		}
	} else {
		p->audio_source = p->video_source;
		obs_source_get_ref(p->audio_source);
	}

	/* 2. Determine resolution */
	p->cx = p->config.width;
	p->cy = p->config.height;
	if (p->cx == 0 || p->cy == 0) {
		p->cx = obs_source_get_width(p->video_source);
		p->cy = obs_source_get_height(p->video_source);
	}
	if (p->cx == 0)
		p->cx = 1920;
	if (p->cy == 0)
		p->cy = 1080;

	int fps_num = p->config.fps_num > 0 ? p->config.fps_num : 30;
	int fps_den = p->config.fps_den > 0 ? p->config.fps_den : 1;

	/* 3. Create virtual video output */
	struct video_output_info voi = {0};
	voi.name = "multi-rec-video";
	voi.format = VIDEO_FORMAT_BGRA;
	voi.fps_num = fps_num;
	voi.fps_den = fps_den;
	voi.width = p->cx;
	voi.height = p->cy;
	voi.cache_size = 6;
	voi.colorspace = VIDEO_CS_SRGB;
	voi.range = VIDEO_RANGE_FULL;

	if (video_output_open(&p->video_output, &voi) != VIDEO_OUTPUT_SUCCESS) {
		set_error(p, "Failed to open virtual video output");
		release_pipeline_objects(p);
		return false;
	}

	/* 4. Create virtual audio output */
	struct audio_output_info aoi = {0};
	aoi.name = "multi-rec-audio";
	aoi.speakers = SPEAKERS_STEREO;
	aoi.samples_per_sec = 48000;
	aoi.format = AUDIO_FORMAT_FLOAT_PLANAR;
	aoi.input_callback = audio_input_callback;
	aoi.input_param = p;

	if (audio_output_open(&p->audio_output, &aoi) != AUDIO_OUTPUT_SUCCESS) {
		set_error(p, "Failed to open virtual audio output");
		release_pipeline_objects(p);
		return false;
	}

	/* 5. Create GPU resources for source capture */
	obs_enter_graphics();
	p->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	p->stagesurface =
		gs_stagesurface_create(p->cx, p->cy, GS_BGRA);
	obs_leave_graphics();

	if (!p->texrender || !p->stagesurface) {
		set_error(p, "Failed to create GPU staging resources");
		release_pipeline_objects(p);
		return false;
	}

	/* 6. Create encoders */
	const char *venc_id = p->config.video_encoder_id;
	if (!venc_id || !*venc_id)
		venc_id = "obs_x264";

	obs_data_t *venc_settings = obs_data_create();
	if (p->config.video_bitrate > 0)
		obs_data_set_int(venc_settings, "bitrate",
				 p->config.video_bitrate);
	obs_data_set_int(venc_settings, "width", p->cx);
	obs_data_set_int(venc_settings, "height", p->cy);

	p->video_encoder = obs_video_encoder_create(
		venc_id, "multi-rec-venc", venc_settings, NULL);
	obs_data_release(venc_settings);

	if (!p->video_encoder) {
		set_error(p, "Failed to create video encoder");
		release_pipeline_objects(p);
		return false;
	}
	obs_encoder_set_video(p->video_encoder, p->video_output);

	const char *aenc_id = p->config.audio_encoder_id;
	if (!aenc_id || !*aenc_id)
		aenc_id = "ffmpeg_aac";

	obs_data_t *aenc_settings = obs_data_create();
	if (p->config.audio_bitrate > 0)
		obs_data_set_int(aenc_settings, "bitrate",
				 p->config.audio_bitrate);

	p->audio_encoder = obs_audio_encoder_create(
		aenc_id, "multi-rec-aenc", aenc_settings, 0, NULL);
	obs_data_release(aenc_settings);

	if (!p->audio_encoder) {
		set_error(p, "Failed to create audio encoder");
		release_pipeline_objects(p);
		return false;
	}
	obs_encoder_set_audio(p->audio_encoder, p->audio_output);

	/* 7. Create file output (muxer) */
	char *output_path =
		record_pipeline_build_path(&p->config,
					   p->config.video_source_name);

	obs_data_t *out_settings = obs_data_create();
	obs_data_set_string(out_settings, "path", output_path);

	/* Choose muxer based on container */
	const char *muxer_id = "ffmpeg_muxer";

	p->file_output = obs_output_create(muxer_id, "multi-rec-output",
					   out_settings, NULL);
	obs_data_release(out_settings);
	bfree(output_path);

	if (!p->file_output) {
		set_error(p, "Failed to create file output");
		release_pipeline_objects(p);
		return false;
	}

	obs_output_set_video_encoder(p->file_output, p->video_encoder);
	obs_output_set_audio_encoder(p->file_output, p->audio_encoder, 0);

	/* Connect signals */
	signal_handler_t *sh =
		obs_output_get_signal_handler(p->file_output);
	signal_handler_connect(sh, "start", on_output_start, p);
	signal_handler_connect(sh, "stop", on_output_stop, p);

	/* 8. Register tick callback to render source each frame */
	obs_add_tick_callback(pipeline_video_tick, p);
	p->tick_registered = true;

	/* 9. Start the output */
	if (!obs_output_start(p->file_output)) {
		const char *err = obs_output_get_last_error(p->file_output);
		set_error(p, err ? err : "Failed to start output");
		release_pipeline_objects(p);
		return false;
	}

	return true;
}

void record_pipeline_stop(struct record_pipeline *pipeline)
{
	if (!pipeline)
		return;

	pthread_mutex_lock(&pipeline->mutex);
	if (pipeline->state != PIPELINE_RECORDING &&
	    pipeline->state != PIPELINE_STARTING) {
		pthread_mutex_unlock(&pipeline->mutex);
		return;
	}
	pipeline->state = PIPELINE_STOPPING;
	pthread_mutex_unlock(&pipeline->mutex);

	if (pipeline->file_output)
		obs_output_stop(pipeline->file_output);

	release_pipeline_objects(pipeline);

	pthread_mutex_lock(&pipeline->mutex);
	pipeline->state = PIPELINE_IDLE;
	pthread_mutex_unlock(&pipeline->mutex);

	if (pipeline->state_callback)
		pipeline->state_callback(pipeline,
					 pipeline->state_callback_param);
}

enum record_pipeline_state
record_pipeline_get_state(const struct record_pipeline *pipeline)
{
	return pipeline ? pipeline->state : PIPELINE_IDLE;
}

const char *record_pipeline_get_error(const struct record_pipeline *pipeline)
{
	return pipeline ? pipeline->last_error : NULL;
}
