#define _GNU_SOURCE

#include "box.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* vertex_buffer = {point0{float, float, float}, point1, ..., point7};
 * element_array_buffer = {
 *  0, 1, // one edge
 *  2, 3,
 *  4, 5, ...
 * };
 *                                        /
 *                    2                  /
 *                    .-------------------------.6
 *                   /|                /       /|
 *                  / |        ^y     /       / |
 *                 /  |        |     /       /  |
 *                /   |        |    /       /   |
 *               /    |        |   /       /    |
 *              /     |        |  /       /     |
 *             /      |        | /       /      |
 *           3.-------------------------.7      |
 * -----------|--------------- / -------|------------>x
 *            |       .-------/-       -|-------.4
 *            |      /0      / |        |      /
 *            |     /       /  |        |     /
 *            |    /       /   |        |    /
 *            |   /       /    |        |   /
 *            |  /       /     |        |  /
 *            | /       /z+    |        | /
 *            |/               |        |/
 *            .-------------------------.5
 *            1                |
 *                             |
 */

/**
 *  pool
 *  <=========for vertex buffer=========> <==for element array buffer==>
 * |      sizeof(struct vertex) * 8      |      sizeof(ushort) * 24     |
 */

static const char vertex_shader[] =
    "#version 410\n"
    "uniform mat4 zMVP;\n"
    "uniform float theta;\n"
    "layout(location = 0) in vec4 position;\n"
    "\n"
    "mat4 rotateY(float a) {\n"
    "  return mat4(\n"
    "      cos(a), 0, sin(a), 0,\n"
    "           0, 1,      0, 0,\n"
    "     -sin(a), 0, cos(a), 0,\n"
    "           0, 0,      0, 1\n"
    "  );\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "  gl_Position = zMVP * rotateY(theta) * position;\n"
    "}\n";

static const char fragment_shader[] =
    "#version 410\n"
    "out vec4 outputColor;\n"
    "void main()\n"
    "{\n"
    "  outputColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";

struct vertex {
  float x, y, z;
};

static int
create_shared_fd(off_t size)
{
  const char *name = "zcms-shm";
  int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) return fd;
  unlink(name);

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

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

static const struct wl_callback_listener frame_callback_listener;

static void
zcms_box_frame_callback_handler(void *data, struct wl_callback *wl_callback, uint32_t time)
{
  struct zcms_box *self = data;
  struct wl_array theta_arr;
  float *theta;
  wl_callback_destroy(wl_callback);

  wl_array_init(&theta_arr);

  theta = wl_array_add(&theta_arr, sizeof(float));
  *theta = (float)(time % 10000) / 10000.0 * 2 * M_PI;  // rotate 2 pi in 10 sec

  zgn_opengl_shader_program_set_uniform_float_vector(self->shader, "theta", 1, 1, &theta_arr);
  zgn_opengl_component_attach_shader_program(self->component, self->shader);

  self->frame_callback = zgn_virtual_object_frame(self->virtual_object);
  wl_callback_add_listener(self->frame_callback, &frame_callback_listener, self);

  zgn_virtual_object_commit(self->virtual_object);

  wl_array_release(&theta_arr);
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = zcms_box_frame_callback_handler,
};

struct zcms_box *
zcms_box_create(struct zcms_app *app, float width, float height, float depth)
{
  struct zcms_box *self;
  struct wl_array half_size, quaternion;
  float *half_size_vec, *quaternion_vec;

  size_t vertex_buffer_size = sizeof(struct vertex) * 8;
  size_t element_array_buffer_size = sizeof(ushort) * 24;
  size_t pool_size = vertex_buffer_size + element_array_buffer_size;
  void *buf;

  self = calloc(1, sizeof *self);
  if (self == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto err;
  }

  self->app = app;
  self->width = width;
  self->height = height;
  self->depth = depth;

  self->virtual_object = zgn_compositor_create_virtual_object(app->compositor);
  assert(self->virtual_object);

  wl_array_init(&half_size);
  wl_array_init(&quaternion);

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

  self->component = zgn_opengl_create_opengl_component(self->app->opengl, self->virtual_object);
  assert(self->component);

  {  // create pool
    int shm_fd = create_shared_fd(pool_size);
    assert(shm_fd > 0);

    buf = mmap(NULL, pool_size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    assert(buf);

    self->buffer_pool = wl_shm_create_pool(self->app->shm, shm_fd, pool_size);
    assert(self->buffer_pool);

    close(shm_fd);
  }

  {  // create vertex buffer
    int i;
    struct vertex *data;
    self->vertex_buffer = zgn_opengl_create_vertex_buffer(self->app->opengl);
    assert(self->vertex_buffer);

    data = buf;
    self->vertex_buffer_buffer =
        wl_shm_pool_create_buffer(self->buffer_pool, 0, vertex_buffer_size, 1, vertex_buffer_size, 0);
    assert(self->vertex_buffer_buffer);
    zgn_opengl_vertex_buffer_attach(self->vertex_buffer, self->vertex_buffer_buffer);

    i = 0;
    for (int x = -1; x < 2; x += 2) {
      for (int y = -1; y < 2; y += 2) {
        for (int z = -1; z < 2; z += 2) {
          data[i].x = x * width / 3;
          data[i].y = y * height / 3;
          data[i].z = z * depth / 3;
          i++;
        }
      }
    }

    zgn_opengl_component_attach_vertex_buffer(self->component, self->vertex_buffer);
  }

  {
    // create element array buffer
    ushort *data;
    ushort element_array[24] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 4, 1, 5, 2, 6, 3, 7, 0, 2, 1, 3, 4, 6, 5, 7};
    self->element_array_buffer = zgn_opengl_create_element_array_buffer(self->app->opengl);
    assert(self->element_array_buffer);

    self->element_array_buffer_buffer = wl_shm_pool_create_buffer(
        self->buffer_pool, vertex_buffer_size, element_array_buffer_size, 1, element_array_buffer_size, 0);
    assert(self->element_array_buffer_buffer);
    zgn_opengl_element_array_buffer_attach(self->element_array_buffer, self->element_array_buffer_buffer,
                                           ZGN_OPENGL_ELEMENT_ARRAY_INDICES_TYPE_UNSIGNED_SHORT);

    data = (ushort *)((int8_t *)buf + vertex_buffer_size);
    memcpy(data, element_array, element_array_buffer_size);

    zgn_opengl_component_attach_element_array_buffer(self->component, self->element_array_buffer);
  }

  {  // create shader program
    char *data;
    self->shader = zgn_opengl_create_shader_program(self->app->opengl);
    assert(self->shader);

    int vertex_shader_source_fd = create_shared_fd(sizeof(vertex_shader));
    assert(vertex_shader_source_fd > 0);
    int fragment_shader_source_fd = create_shared_fd(sizeof(fragment_shader));
    assert(fragment_shader_source_fd > 0);

    data = mmap(NULL, sizeof(vertex_shader), PROT_WRITE, MAP_SHARED, vertex_shader_source_fd, 0);
    assert(data);

    memcpy(data, vertex_shader, sizeof(vertex_shader));
    munmap(data, sizeof(vertex_shader));

    data = mmap(NULL, sizeof(fragment_shader), PROT_WRITE, MAP_SHARED, fragment_shader_source_fd, 0);
    assert(data);

    memcpy(data, fragment_shader, sizeof(fragment_shader));
    munmap(data, sizeof(fragment_shader));

    zgn_opengl_shader_program_set_vertex_shader(self->shader, vertex_shader_source_fd, sizeof(vertex_shader));
    zgn_opengl_shader_program_set_fragment_shader(self->shader, fragment_shader_source_fd,
                                                  sizeof(fragment_shader));
    zgn_opengl_shader_program_link(self->shader);

    close(vertex_shader_source_fd);
    close(fragment_shader_source_fd);

    zgn_opengl_component_attach_shader_program(self->component, self->shader);
  }

  zgn_opengl_component_set_count(self->component, 24);
  zgn_opengl_component_set_topology(self->component, ZGN_OPENGL_TOPOLOGY_LINES);
  zgn_opengl_component_add_vertex_attribute(self->component, 0, 3, ZGN_OPENGL_VERTEX_ATTRIBUTE_TYPE_FLOAT,
                                            false, sizeof(struct vertex), 0);

  {  // clean
    munmap(buf, pool_size);
  }

  self->frame_callback = zgn_virtual_object_frame(self->virtual_object);
  wl_callback_add_listener(self->frame_callback, &frame_callback_listener, self);

  zgn_virtual_object_commit(self->virtual_object);

  return self;

err:
  return NULL;
}

void
zcms_box_destroy(struct zcms_box *self)
{
  zgn_opengl_shader_program_destroy(self->shader);
  zgn_opengl_vertex_buffer_destroy(self->vertex_buffer);
  wl_buffer_destroy(self->vertex_buffer_buffer);
  wl_shm_pool_destroy(self->buffer_pool);
  zgn_opengl_component_destroy(self->component);
  zgn_cuboid_window_destroy(self->cuboid_window);
  zgn_virtual_object_destroy(self->virtual_object);
  free(self);
}
