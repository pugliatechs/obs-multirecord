#pragma once

#include <obs-module.h>
#include <media-io/audio-io.h>
#include <media-io/video-io.h>
#include <util/threading.h>

#ifdef __cplusplus
extern "C" {
#endif

enum record_pipeline_state {
	PIPELINE_IDLE = 0,
	PIPELINE_STARTING,
	PIPELINE_RECORDING,
	PIPELINE_STOPPING,
	PIPELINE_ERROR,
};

struct record_pipeline_config {
	char *video_source_name;

	char *output_dir;
	char *filename_format;
	char *container;

	char *video_encoder_id;
	char *audio_encoder_id;

	int video_bitrate;
	int audio_bitrate;

	int width;
	int height;
	int fps_num;
	int fps_den;
};

struct record_pipeline {
	struct record_pipeline_config config;

	/* Source reference */
	obs_source_t *video_source;

	/* View-based rendering (no GPU readback needed) */
	obs_view_t *view;
	video_t *view_video;

	/* Encoders and output */
	obs_encoder_t *video_encoder;
	obs_encoder_t *audio_encoder;
	obs_output_t *file_output;

	/* Resolution */
	uint32_t cx;
	uint32_t cy;

	/* State */
	volatile enum record_pipeline_state state;
	pthread_mutex_t mutex;

	void (*state_callback)(struct record_pipeline *pipeline, void *param);
	void *state_callback_param;

	bool source_showing;
	char *last_error;
};

/* Lifecycle */
struct record_pipeline *record_pipeline_create(
	const struct record_pipeline_config *config);
void record_pipeline_destroy(struct record_pipeline *pipeline);

/* Control */
bool record_pipeline_start(struct record_pipeline *pipeline);
void record_pipeline_stop(struct record_pipeline *pipeline);
enum record_pipeline_state
record_pipeline_get_state(const struct record_pipeline *pipeline);
const char *record_pipeline_get_error(const struct record_pipeline *pipeline);

/* Helpers */
char *record_pipeline_build_path(const struct record_pipeline_config *config,
				 const char *source_name);
size_t record_pipeline_enum_video_encoders(
	bool (*cb)(void *param, const char *id, const char *name),
	void *param);
size_t record_pipeline_enum_audio_encoders(
	bool (*cb)(void *param, const char *id, const char *name),
	void *param);

/* Config */
void record_pipeline_config_init(struct record_pipeline_config *cfg);
void record_pipeline_config_copy(struct record_pipeline_config *dst,
				 const struct record_pipeline_config *src);
void record_pipeline_config_free(struct record_pipeline_config *cfg);

#ifdef __cplusplus
}
#endif
