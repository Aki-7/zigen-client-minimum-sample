#include "box.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void
cuboid_window_configure(void *data, struct zgn_cuboid_window *zgn_cuboid_window, uint32_t serial,
                        struct wl_array *half_size, struct wl_array *quaternion)
{
  struct zcms_box *self = data;
  (void)zgn_cuboid_window;
  (void)half_size;
  (void)quaternion;
  zgn_cuboid_window_ack_configure(self->cuboid_window, serial);
}

static void
cuboid_window_move(void *data, struct zgn_cuboid_window *zgn_cuboid_window, struct wl_array *face_direction)
{
  (void)data;
  (void)zgn_cuboid_window;
  (void)face_direction;
}

static const struct zgn_cuboid_window_listener cuboid_window_listener = {
    .configure = cuboid_window_configure,
    .moved = cuboid_window_move,
};

struct zcms_box *
zcms_box_create(struct zcms_app *app, float width, float height, float depth)
{
  struct zcms_box *self;
  struct wl_array half_size, quaternion;
  // float *half_size_vec[3], *quaternion_vec[4];
  float *half_size_vec, *quaternion_vec;

  self = calloc(1, sizeof *app);
  if (self == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto err;
  }

  self->virtual_object = zgn_compositor_create_virtual_object(app->compositor);
  assert(self->virtual_object);

  wl_array_init(&half_size);
  wl_array_init(&quaternion);

  // *half_size_vec = wl_array_add(&half_size, sizeof(float) * 3);
  // *half_size_vec = (float[3]){width, height, depth};

  // *quaternion_vec = wl_array_add(&quaternion, sizeof(float) * 4);
  // *quaternion_vec = (float[4]){0, 0, 0, 1};

  half_size_vec = wl_array_add(&half_size, sizeof(float) * 3);
  half_size_vec[0] = width / 2;
  half_size_vec[1] = height / 2;
  half_size_vec[2] = depth / 2;

  quaternion_vec = wl_array_add(&quaternion, sizeof(float) * 4);
  quaternion_vec[0] = 0;
  quaternion_vec[1] = 0;
  quaternion_vec[2] = 0;
  quaternion_vec[3] = 1;

  self->cuboid_window =
      zgn_shell_get_cuboid_window(app->shell, self->virtual_object, &half_size, &quaternion);
  assert(self->cuboid_window);

  zgn_cuboid_window_add_listener(self->cuboid_window, &cuboid_window_listener, self);

  self->app = app;
  self->width = width;
  self->height = height;
  self->depth = depth;

  return self;

err:
  return NULL;
}

void
zcms_box_destroy(struct zcms_box *self)
{
  zgn_virtual_object_destroy(self->virtual_object);
  free(self);
}
