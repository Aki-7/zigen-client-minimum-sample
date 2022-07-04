#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "app.h"
#include "box.h"

int
main()
{
  struct zcms_app *app;
  struct zcms_box *box;
  struct wl_display *display;
  int status = EXIT_FAILURE;
  const char *socket = "zigen-0";

  display = wl_display_connect(socket);
  if (display == NULL) {
    fprintf(stderr, "Failed to create display\n");
    goto err;
  }

  app = zcms_app_create(display);
  if (app == NULL) {
    fprintf(stderr, "Failed to create app\n");
    goto err_display;
  }

  box = zcms_box_create(app, 0.1, 0.1, 0.1);
  if (box == NULL) {
    fprintf(stderr, "Failed to create a box\n");
    goto err_app;
  }

  if (zcms_app_run(app) != 0) goto err_box;

  status = EXIT_SUCCESS;

err_box:
  zcms_box_destroy(box);

err_app:
  zcms_app_destroy(app);

err_display:
  wl_display_disconnect(display);

err:
  fprintf(stderr, "Exited gracefully\n");
  return status;
}
