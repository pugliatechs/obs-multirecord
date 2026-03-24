#include "record-pipeline.h"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <time.h>
#include <string.h>

#define LOG_PREFIX "[obs-multi-record] "

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
	cfg->filename_format = bstrdup("%N_%Y%m%d_%H%M%S");
}

static char *safe_strdup(const char *s)
{
	return s ? bstrdup(s) : NULL;
}

void record_pipeline_config_copy(struct record_pipeline_config *dst,
				 const struct record_pipeline_config *src)
{
	dst->video_source_name = safe_strdup(src->video_source_name);
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
	bfree(cfg->output_dir);
	bfree(cfg->filename_format);
	bfree(cfg->container);
	bfree(cfg->video_encoder_id);
	bfree(cfg->audio_encoder_id);
	memset(cfg, 0, sizeof(*cfg));
}

/* ------------------------------------------------------------------ */
/* Path builder (%N=name, %Y%m%d%H%M%S=datetime)                     */
/* ------------------------------------------------------------------ */

static void sanitize_name(struct dstr *out, const char *name)
{
	if (!name || !*name) {
		dstr_cat(out, "unknown");
		return;
	}
	for (const char *s = name; *s; s++) {
		char c = *s;
		if (c == '/' || c == '\\' || c == ':' || c == '*' ||
		    c == '?' || c == '"' || c == '<' || c == '>' ||
		    c == '|' || c == '.' || c == ' ' ||
		    (unsigned char)c < 0x20)
			c = '_';
		dstr_cat_ch(out, c);
	}
}

char *record_pipeline_build_path(const struct record_pipeline_config *config,
				 const char *source_name)
{
	struct dstr path = {0};
	time_t now = time(NULL);
	struct tm tm_info;
#ifdef _WIN32
	localtime_s(&tm_info, &now);
#else
	localtime_r(&now, &tm_info);
#endif

	const char *fmt = config->filename_format;
	if (!fmt || !*fmt)
		fmt = "%N_%Y%m%d_%H%M%S";

	struct dstr filename = {0};
	for (const char *p = fmt; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			p++;
			switch (*p) {
			case 'N': sanitize_name(&filename, source_name); break;
			case 'Y': dstr_catf(&filename, "%04d", tm_info.tm_year + 1900); break;
			case 'm': dstr_catf(&filename, "%02d", tm_info.tm_mon + 1); break;
			case 'd': dstr_catf(&filename, "%02d", tm_info.tm_mday); break;
			case 'H': dstr_catf(&filename, "%02d", tm_info.tm_hour); break;
			case 'M': dstr_catf(&filename, "%02d", tm_info.tm_min); break;
			case 'S': dstr_catf(&filename, "%02d", tm_info.tm_sec); break;
			case '%': dstr_cat_ch(&filename, '%'); break;
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
	if (!dir || !*dir) dir = ".";
	const char *ext = config->container;
	if (!ext || !*ext) ext = "mkv";

	dstr_printf(&path, "%s/%s.%s", dir, filename.array, ext);
	dstr_free(&filename);
	return path.array;
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
		if (!name) name = id;
		count++;
		if (cb && !cb(param, id, name)) break;
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
		if (!name) name = id;
		count++;
		if (cb && !cb(param, id, name)) break;
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
	bool notify = false;

	pthread_mutex_lock(&p->mutex);
	if (p->state == PIPELINE_RECORDING || p->state == PIPELINE_STOPPING) {
		p->state = PIPELINE_IDLE;
		notify = true;
	}
	pthread_mutex_unlock(&p->mutex);

	blog(LOG_INFO, LOG_PREFIX "Output stopped for '%s'",
	     p->config.video_source_name ? p->config.video_source_name : "?");

	if (notify && p->state_callback)
		p->state_callback(p, p->state_callback_param);
}

static void on_output_start(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	struct record_pipeline *p = param;

	pthread_mutex_lock(&p->mutex);
	p->state = PIPELINE_RECORDING;
	pthread_mutex_unlock(&p->mutex);

	blog(LOG_INFO, LOG_PREFIX "Output started for '%s'",
	     p->config.video_source_name ? p->config.video_source_name : "?");

	if (p->state_callback)
		p->state_callback(p, p->state_callback_param);
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
	blog(LOG_ERROR, LOG_PREFIX "%s (source: %s)", msg,
	     p->config.video_source_name ? p->config.video_source_name : "?");
	bfree(p->last_error);
	p->last_error = bstrdup(msg);
	p->state = PIPELINE_ERROR;

	if (p->state_callback)
		p->state_callback(p, p->state_callback_param);
}

static void release_pipeline_objects(struct record_pipeline *p)
{
	/* 1. Disconnect output signals and release output */
	if (p->file_output) {
		signal_handler_t *sh =
			obs_output_get_signal_handler(p->file_output);
		signal_handler_disconnect(sh, "start", on_output_start, p);
		signal_handler_disconnect(sh, "stop", on_output_stop, p);
		obs_output_release(p->file_output);
		p->file_output = NULL;
	}

	/* 2. Release encoders */
	if (p->video_encoder) {
		obs_encoder_release(p->video_encoder);
		p->video_encoder = NULL;
	}
	if (p->audio_encoder) {
		obs_encoder_release(p->audio_encoder);
		p->audio_encoder = NULL;
	}

	/* 3. Remove view from render loop and destroy */
	if (p->view) {
		obs_view_set_source(p->view, 0, NULL);
		obs_view_remove(p->view);
		obs_view_destroy(p->view);
		p->view = NULL;
		p->view_video = NULL;
		blog(LOG_INFO, LOG_PREFIX "View destroyed");
	}

	/* 4. Deactivate and release source */
	if (p->source_showing && p->video_source) {
		obs_source_dec_showing(p->video_source);
		obs_source_dec_active(p->video_source);
		p->source_showing = false;
	}

	if (p->video_source) {
		obs_source_release(p->video_source);
		p->video_source = NULL;
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

	blog(LOG_INFO, LOG_PREFIX "Starting pipeline for '%s'",
	     p->config.video_source_name ? p->config.video_source_name : "?");

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

	/* 2. Activate source */
	obs_source_inc_active(p->video_source);
	obs_source_inc_showing(p->video_source);
	p->source_showing = true;

	/* 3. Determine resolution (native source size) */
	p->cx = obs_source_get_width(p->video_source);
	p->cy = obs_source_get_height(p->video_source);
	if (p->cx == 0 || p->cy == 0) {
		for (int retry = 0; retry < 10; retry++) {
			os_sleep_ms(100);
			p->cx = obs_source_get_width(p->video_source);
			p->cy = obs_source_get_height(p->video_source);
			if (p->cx > 0 && p->cy > 0)
				break;
		}
	}
	if (p->cx == 0 || p->cy == 0) {
		set_error(p, "Source reports 0x0 resolution");
		release_pipeline_objects(p);
		return false;
	}

	int fps_num = p->config.fps_num > 0 ? p->config.fps_num : 30;
	int fps_den = p->config.fps_den > 0 ? p->config.fps_den : 1;

	blog(LOG_INFO, LOG_PREFIX "Resolution: %ux%u @ %d/%d fps",
	     p->cx, p->cy, fps_num, fps_den);

	/* 4. Create an obs_view to render the source.
	 *    This is the approach used by obs-source-record:
	 *    the view renders the source natively into its own
	 *    video_t output, with no GPU readback needed. */
	p->view = obs_view_create();
	if (!p->view) {
		set_error(p, "Failed to create obs_view");
		release_pipeline_objects(p);
		return false;
	}

	/* Set the source on channel 0 of the view */
	obs_view_set_source(p->view, 0, p->video_source);

	/* Add the view to the render loop with custom video settings */
	struct obs_video_info ovi = {0};
	ovi.fps_num = fps_num;
	ovi.fps_den = fps_den;
	ovi.base_width = p->cx;
	ovi.base_height = p->cy;
	ovi.output_width = p->cx;
	ovi.output_height = p->cy;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;

	p->view_video = obs_view_add2(p->view, &ovi);
	if (!p->view_video) {
		set_error(p, "Failed to add view to render loop");
		release_pipeline_objects(p);
		return false;
	}

	blog(LOG_INFO, LOG_PREFIX "View created and added to render loop");

	/* 5. Create video encoder (connects to view's video output) */
	const char *venc_id = p->config.video_encoder_id;
	if (!venc_id || !*venc_id)
		venc_id = "obs_x264";

	obs_data_t *venc_settings = obs_data_create();
	if (p->config.video_bitrate > 0)
		obs_data_set_int(venc_settings, "bitrate",
				 p->config.video_bitrate);

	p->video_encoder = obs_video_encoder_create(
		venc_id, "multi-rec-venc", venc_settings, NULL);
	obs_data_release(venc_settings);

	if (!p->video_encoder) {
		set_error(p, "Failed to create video encoder");
		release_pipeline_objects(p);
		return false;
	}
	/* Connect encoder to the view's video output (not OBS main) */
	obs_encoder_set_video(p->video_encoder, p->view_video);

	/* 6. Create audio encoder (connects to OBS main audio) */
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
	/* Use OBS main audio output for reliable audio capture */
	obs_encoder_set_audio(p->audio_encoder, obs_get_audio());

	/* 8. Create file output */
	char *output_path =
		record_pipeline_build_path(&p->config,
					   p->config.video_source_name);

	/* Validate output directory exists */
	{
		char real_dir[512];
		if (!os_get_abs_path(p->config.output_dir, real_dir,
				     sizeof(real_dir))) {
			bfree(output_path);
			set_error(p, "Output directory does not exist");
			release_pipeline_objects(p);
			return false;
		}
	}

	blog(LOG_INFO, LOG_PREFIX "Output path: %s", output_path);

	obs_data_t *out_settings = obs_data_create();
	obs_data_set_string(out_settings, "path", output_path);

	p->file_output = obs_output_create("ffmpeg_muxer", "multi-rec-output",
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

	signal_handler_t *sh =
		obs_output_get_signal_handler(p->file_output);
	signal_handler_connect(sh, "start", on_output_start, p);
	signal_handler_connect(sh, "stop", on_output_stop, p);

	/* 9. Start the output */
	if (!obs_output_start(p->file_output)) {
		const char *err = obs_output_get_last_error(p->file_output);
		blog(LOG_ERROR, LOG_PREFIX "obs_output_start failed: %s",
		     err ? err : "unknown");
		set_error(p, err ? err : "Failed to start output");
		release_pipeline_objects(p);
		return false;
	}

	blog(LOG_INFO, LOG_PREFIX "Pipeline started successfully");
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

	blog(LOG_INFO, LOG_PREFIX "Stopping pipeline for '%s'",
	     pipeline->config.video_source_name
		     ? pipeline->config.video_source_name
		     : "?");

	/* Force-stop output first */
	if (pipeline->file_output) {
		obs_output_force_stop(pipeline->file_output);
		blog(LOG_INFO, LOG_PREFIX "Output force-stopped");
	}

	release_pipeline_objects(pipeline);

	pthread_mutex_lock(&pipeline->mutex);
	pipeline->state = PIPELINE_IDLE;
	pthread_mutex_unlock(&pipeline->mutex);

	if (pipeline->state_callback)
		pipeline->state_callback(pipeline,
					 pipeline->state_callback_param);

	blog(LOG_INFO, LOG_PREFIX "Pipeline stopped");
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
