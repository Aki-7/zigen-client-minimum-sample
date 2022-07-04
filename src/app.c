#include "app.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface,
                uint32_t version)
{
  struct zcms_app *self = data;
  if (strcmp(interface, "wl_shm") == 0) {
    self->shm = wl_registry_bind(registry, id, &wl_shm_interface, version);
  } else if (strcmp(interface, "zgn_compositor") == 0) {
    self->compositor = wl_registry_bind(registry, id, &zgn_compositor_interface, version);
  } else if (strcmp(interface, "zgn_shell") == 0) {
    self->shell = wl_registry_bind(registry, id, &zgn_shell_interface, version);
  } else if (strcmp(interface, "zgn_opengl") == 0) {
    self->opengl = wl_registry_bind(registry, id, &zgn_opengl_interface, version);
  }
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static int
zcms_app_dispatch(struct zcms_app *self)
{
  while (wl_display_prepare_read(self->display) != 0) {
    if (errno != EAGAIN) return -1;
    wl_display_dispatch_pending(self->display);
  }

  if (wl_display_flush(self->display) == -1) return -1;
  wl_display_read_events(self->display);
  wl_display_dispatch_pending(self->display);

  if (wl_display_flush(self->display) == -1) return -1;

  return 0;
}

static int
zcms_app_poll(struct zcms_app *self)
{
  int epoll_count;
  int ret;
  struct epoll_event events[16];

  epoll_count = epoll_wait(self->epoll_fd, events, 16, -1);
  for (int i = 0; i < epoll_count; i++) {
    assert(events[i].data.ptr == self);
    ret = zcms_app_dispatch(self);
    if (ret != 0) return ret;
  }

  return 0;
}

int
zcms_app_run(struct zcms_app *self)
{
  if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, wl_display_get_fd(self->display), &self->epoll_event) == -1) {
    fprintf(stderr, "Failed to add wayland event fd to epoll fd\n");
    return -1;
  }

  wl_display_flush(self->display);

  self->running = true;
  while (self->running) {
    if (zcms_app_poll(self) != 0) return -1;
  }

  return 0;
}

struct zcms_app *
zcms_app_create(struct wl_display *display)
{
  struct zcms_app *self;

  self = calloc(1, sizeof *self);
  if (self == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto err;
  }

  self->display = display;
  self->epoll_event.data.ptr = self;
  self->epoll_event.events = EPOLLIN;
  self->running = false;

  self->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (self->epoll_fd == -1) {
    fprintf(stderr, "Failed to create epoll fd\n");
    goto err_free;
  }

  self->registry = wl_display_get_registry(display);
  if (self->registry == NULL) goto err_close;

  wl_registry_add_listener(self->registry, &registry_listener, self);

  wl_display_dispatch(display);
  wl_display_roundtrip(display);

  if (self->shm == NULL || self->compositor == NULL || self->opengl == NULL || self->shell == NULL)
    goto err_globals;

  return self;

err_globals:
  if (self->shm) wl_shm_destroy(self->shm);
  if (self->compositor) zgn_compositor_destroy(self->compositor);
  if (self->opengl) zgn_opengl_destroy(self->opengl);
  if (self->shell) zgn_shell_destroy(self->shell);

err_close:
  close(self->epoll_fd);

err_free:
  free(self);

err:
  return NULL;
}

void
zcms_app_destroy(struct zcms_app *self)
{
  wl_shm_destroy(self->shm);
  zgn_compositor_destroy(self->compositor);
  zgn_opengl_destroy(self->opengl);
  zgn_shell_destroy(self->shell);
  close(self->epoll_fd);
  free(self);
}
