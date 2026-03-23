#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-multi-record", "en-US")
OBS_MODULE_AUTHOR("PugliaTechs")

/* Defined in multi-record-dock.cpp (C++ linkage) */
extern void multi_record_dock_register(void);

static void on_frontend_event(enum obs_frontend_event event, void *data)
{
	(void)data;

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		multi_record_dock_register();
	}
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(on_frontend_event, NULL);

	blog(LOG_INFO,
	     "[obs-multi-record] Plugin loaded (version %s)",
	     "1.0.0");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[obs-multi-record] Plugin unloaded");
}

const char *obs_module_description(void)
{
	return "Record individual video+audio source pairs to separate files, "
	       "independent of the main program recording.";
}
