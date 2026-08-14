
#include "appglance.h"

#if PBL_API_EXISTS(app_glance_reload)
static void prv_update_app_glance(AppGlanceReloadSession *session,
                                  size_t limit, void *context) {
  // This should never happen, but developers should always ensure they are
  // not adding more slices than are available
  if (limit < 1) return;

  // Cast the context object to a string
  const char *message = context;

  if(message) {
    // Create the AppGlanceSlice
    // NOTE: When .icon_resource_id is not set, the app's default icon is used
    const AppGlanceSlice entry = (AppGlanceSlice) {
      .layout = {
        .icon = PUBLISHED_ID_LOGO,
        .subtitle_template_string = message
      },
      .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION
    };

    // Add the slice, and check the result
    const AppGlanceResult result = app_glance_add_slice(session, entry);

    if (result != APP_GLANCE_RESULT_SUCCESS) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "AppGlance Error: %d", result);
    }
  }
}
#endif

void update_app_glance(char *text) {
#if PBL_API_EXISTS(app_glance_reload)
#ifdef PBL_ROUND
  app_glance_reload(prv_update_app_glance,&text[4]);
#else
  app_glance_reload(prv_update_app_glance,text);
#endif
#endif
}
