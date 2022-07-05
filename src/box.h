#ifndef ZCMS_BOX_H
#define ZCMS_BOX_H

#include <zigen-client-protocol.h>
#include <zigen-opengl-client-protocol.h>
#include <zigen-shell-client-protocol.h>

#include "app.h"

struct zcms_box {
  struct zcms_app *app;
  float width, height, depth;

  struct zgn_virtual_object *virtual_object;
  struct zgn_cuboid_window *cuboid_window;
  struct zgn_opengl_component *component;

  struct wl_shm_pool *buffer_pool;

  struct zgn_opengl_vertex_buffer *vertex_buffer;
  struct wl_buffer *vertex_buffer_buffer;

  struct zgn_opengl_element_array_buffer *element_array_buffer;
  struct wl_buffer *element_array_buffer_buffer;

  struct zgn_opengl_shader_program *shader;
};

struct zcms_box *
zcms_box_create(struct zcms_app *app, float width, float height, float depth);

void
zcms_box_destroy(struct zcms_box *self);

#endif  //  ZCMS_BOX_H
