#ifndef ZCMS_APP_H
#define ZCMS_APP_H

#include <stdbool.h>
#include <sys/epoll.h>
#include <wayland-client.h>
#include <zigen-client-protocol.h>
#include <zigen-opengl-client-protocol.h>
#include <zigen-shell-client-protocol.h>

struct zcms_app {
  struct wl_display *display;
  struct wl_registry *registry;

  struct wl_shm *shm;
  struct zgn_compositor *compositor;
  struct zgn_opengl *opengl;
  struct zgn_shell *shell;

  struct epoll_event epoll_event;
  int epoll_fd;
  bool running;
};

int
zcms_app_run(struct zcms_app *self);

struct zcms_app *
zcms_app_create(struct wl_display *display);

void
zcms_app_destroy(struct zcms_app *self);

#endif  //  ZCMS_APP_H
