#pragma once

#include <obs-module.h>
#include <media-io/audio-io.h>
#include <media-io/video-io.h>
#include <util/threading.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Per-source recording pipeline.
 *
 * Each instance owns:
 *   source(s) → virtual video/audio output → encoder → file output (muxer)
 */

enum record_pipeline_state {
	PIPELINE_IDLE = 0,
	PIPELINE_STARTING,
	PIPELINE_RECORDING,
	PIPELINE_STOPPING,
	PIPELINE_ERROR,
};

struct record_pipeline_config {
	/* Source names (resolved at start time) */
	char *video_source_name;
	char *audio_source_name; /* may be NULL → use video source audio */

	/* Output file */
	char *output_dir;
	char *filename_format; /* e.g. "%S_%Y%m%d_%H%M%S" (%S = source name) */
	char *container;       /* "mkv", "mp4", "mov", "ts" */

	/* Encoder IDs (e.g. "obs_x264", "jim_nvenc", "ffmpeg_aac") */
	char *video_encoder_id;
	char *audio_encoder_id;

	/* Encoder settings */
	int video_bitrate; /* kbps, 0 = encoder default */
	int audio_bitrate; /* kbps, 0 = encoder default */

	/* Resolution override (0 = match source) */
	int width;
	int height;
	int fps_num;
	int fps_den;
};

struct record_pipeline {
	/* Config (owned, deep-copied on create) */
	struct record_pipeline_config config;

	/* OBS objects — created on start, released on stop */
	obs_source_t *video_source;
	obs_source_t *audio_source;

	video_t *video_output;
	audio_t *audio_output;

	obs_encoder_t *video_encoder;
	obs_encoder_t *audio_encoder;

	obs_output_t *file_output;

	/* Render target for capturing source video */
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	uint32_t cx;
	uint32_t cy;

	/* State */
	volatile enum record_pipeline_state state;
	pthread_mutex_t mutex;

	/* Callback for state changes (called from any thread) */
	void (*state_callback)(struct record_pipeline *pipeline, void *param);
	void *state_callback_param;

	/* Internal: video tick registered flag */
	bool tick_registered;

	/* Error message (valid when state == PIPELINE_ERROR) */
	char *last_error;
};

/* --- Lifecycle --- */

/**
 * Allocate and initialise a pipeline with the given config.
 * Does NOT start recording — call record_pipeline_start() for that.
 */
struct record_pipeline *record_pipeline_create(
	const struct record_pipeline_config *config);

/** Deep-free the pipeline. Stops recording first if active. */
void record_pipeline_destroy(struct record_pipeline *pipeline);

/* --- Control --- */

/** Resolve sources, create encoders/output, begin recording. */
bool record_pipeline_start(struct record_pipeline *pipeline);

/** Gracefully stop recording and release OBS objects. */
void record_pipeline_stop(struct record_pipeline *pipeline);

/** Current state (lock-free read). */
enum record_pipeline_state
record_pipeline_get_state(const struct record_pipeline *pipeline);

/** Last error string (valid only when state == ERROR). Do NOT free. */
const char *record_pipeline_get_error(const struct record_pipeline *pipeline);

/* --- Helpers --- */

/**
 * Build the full output file path from config + current time.
 * Caller must bfree() the result.
 */
char *record_pipeline_build_path(const struct record_pipeline_config *config,
				 const char *source_name);

/**
 * Enumerate available video encoder IDs into a caller-provided callback.
 * Returns the number of encoders found.
 */
size_t record_pipeline_enum_video_encoders(
	bool (*cb)(void *param, const char *id, const char *name),
	void *param);

/**
 * Enumerate available audio encoder IDs.
 */
size_t record_pipeline_enum_audio_encoders(
	bool (*cb)(void *param, const char *id, const char *name),
	void *param);

/* --- Config helpers --- */

void record_pipeline_config_init(struct record_pipeline_config *cfg);
void record_pipeline_config_copy(struct record_pipeline_config *dst,
				 const struct record_pipeline_config *src);
void record_pipeline_config_free(struct record_pipeline_config *cfg);

#ifdef __cplusplus
}
#endif
