/*
 * GStreamer Plugins VR
 * Copyright (C) 2016 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define GST_USE_UNSTABLE_API
#include <gst/gl/gl.h>

#include "gst3dmesh.h"

#define GST_CAT_DEFAULT gst_3d_mesh_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

G_DEFINE_TYPE_WITH_CODE (Gst3DMesh, gst_3d_mesh, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_3d_mesh_debug, "3dmesh", 0, "mesh"));

void
gst_3d_mesh_init (Gst3DMesh * self)
{
  self->index_size = 0;
  self->vao = 0;
  self->vbo_positions = 0;
  self->vbo_indices = 0;
  self->vbo_uv = 0;
}

Gst3DMesh *
gst_3d_mesh_new (GstGLContext * context)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DMesh *mesh = g_object_new (GST_3D_TYPE_MESH, NULL);
  mesh->context = gst_object_ref (context);
  return mesh;
}

Gst3DMesh *
gst_3d_mesh_new_sphere (GstGLContext * context, float radius, unsigned stacks,
    unsigned slices)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DMesh *mesh = gst_3d_mesh_new (context);
  gst_3d_mesh_init_buffers (mesh);
  gst_3d_mesh_upload_sphere (mesh, 10.0, 20, 20);
  return mesh;
}

Gst3DMesh *
gst_3d_mesh_new_plane (GstGLContext * context, float aspect)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DMesh *mesh = gst_3d_mesh_new (context);
  gst_3d_mesh_init_buffers (mesh);
  gst_3d_mesh_upload_plane (mesh, aspect);
  return mesh;
}


Gst3DMesh *
gst_3d_mesh_new_point_plane (GstGLContext * context, unsigned width,
    unsigned height)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DMesh *mesh = gst_3d_mesh_new (context);
  gst_3d_mesh_init_buffers (mesh);
  gst_3d_mesh_upload_point_plane (mesh, width, height);
  return mesh;
}

Gst3DMesh *
gst_3d_mesh_new_line (GstGLContext * context, graphene_vec3_t * from,
    graphene_vec3_t * to, graphene_vec3_t * color)
{
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  Gst3DMesh *mesh = gst_3d_mesh_new (context);
  gst_3d_mesh_init_buffers (mesh);
  gst_3d_mesh_upload_line (mesh, from, to, color);
  return mesh;
}

static void
gst_3d_mesh_finalize (GObject * object)
{
  Gst3DMesh *self = GST_3D_MESH (object);
  g_return_if_fail (self != NULL);

  GstGLFuncs *gl = self->context->gl_vtable;
  if (self->vao) {
    gl->DeleteVertexArrays (1, &self->vao);
    self->vao = 0;
  }

  if (self->vbo_positions) {
    gl->DeleteBuffers (1, &self->vbo_positions);
    self->vbo_positions = 0;
  }

  if (self->vbo_indices) {
    gl->DeleteBuffers (1, &self->vbo_indices);
    self->vbo_indices = 0;
  }

  if (self->context) {
    gst_object_unref (self->context);
    self->context = NULL;
  }

  G_OBJECT_CLASS (gst_3d_mesh_parent_class)->finalize (object);
}

static void
gst_3d_mesh_class_init (Gst3DMeshClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  obj_class->finalize = gst_3d_mesh_finalize;
}


gboolean
gst_3d_mesh_has_buffers (Gst3DMesh * self)
{
  if (self->vbo_positions)
    return TRUE;
  return FALSE;
}

void
gst_3d_mesh_init_buffers (Gst3DMesh * self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GST_IS_GL_CONTEXT (self->context));

  GstGLFuncs *gl = self->context->gl_vtable;

  gl->GenVertexArrays (1, &self->vao);
  gl->BindVertexArray (self->vao);

  gl->GenBuffers (1, &self->vbo_indices);
/*
  gl->GenBuffers (1, &self->vbo_positions);
  gl->GenBuffers (1, &self->vbo_uv);

  GST_DEBUG ("generating mesh. vao: %d pos: %d uv: %d index: %d", self->vao,
      self->vbo_positions, self->vbo_uv, self->vbo_indices);
*/
}

void
gst_3d_mesh_bind (Gst3DMesh * self)
{
  GstGLFuncs *gl = self->context->gl_vtable;
  gl->BindVertexArray (self->vao);
}

void
gst_3d_mesh_bind_buffers (Gst3DMesh * self, GLint attr_position, GLint attr_uv)
{
  GstGLFuncs *gl = self->context->gl_vtable;



  GList *l;
  for (l = self->attribute_buffers; l != NULL; l = l->next) {
    struct Gst3DAttributeBuffer *buf = (struct Gst3DAttributeBuffer *) l->data;
    GST_ERROR ("buffer: %s, %d, %d, %zu", buf->name, buf->location,
        buf->vector_length, buf->element_size);
    //GST_ERROR ("buffer: %s", buf->name);
    //GST_ERROR ("location: %d", buf->location);

    /*
     */
    gl->BindBuffer (GL_ARRAY_BUFFER, buf->location);

    GLint attrib_location;

    if (g_strcmp0 (l->data, "position") == 0) {
      attrib_location = attr_position;
    } else if (g_strcmp0 (l->data, "uv") == 0) {
      attr_position = attr_uv;
    }
    gl->VertexAttribPointer (attrib_location, self->vector_length, GL_FLOAT,
        GL_FALSE, buf->vector_length * sizeof (GLfloat), 0);
  }

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, self->vbo_indices);

/*
  // Load the vertex positions
  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_positions);
  gl->VertexAttribPointer (attr_position,
      self->vector_length,
      GL_FLOAT, GL_FALSE, self->vector_length * sizeof (GLfloat), 0);

  if (attr_uv != -1) {
    gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_uv);
    // Load the texture coordinates
    gl->VertexAttribPointer (attr_uv,
        2, GL_FLOAT, GL_FALSE, 2 * sizeof (GLfloat), 0);
  }

*/
}

void
gst_3d_mesh_unbind_buffers (Gst3DMesh * self)
{
  GstGLFuncs *gl = self->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
}


void
gst_3d_mesh_upload_sphere (Gst3DMesh * self, float radius, unsigned stacks,
    unsigned slices)
{
  GLfloat *vertices;
  GLfloat *texcoords;
  GLushort *indices;

  GstGLFuncs *gl = self->context->gl_vtable;

  self->vector_length = 3;
  int vertex_count = (slices + 1) * stacks;
  const int component_size = sizeof (GLfloat) * vertex_count;

  vertices = (GLfloat *) malloc (component_size * 3);
  texcoords = (GLfloat *) malloc (component_size * 2);

  GLfloat *v = vertices;
  GLfloat *t = texcoords;

  float const J = 1. / (float) (stacks - 1);
  float const I = 1. / (float) (slices - 1);

  for (int i = 0; i < slices + 1; i++) {
    float const theta = M_PI * i * I;
    for (int j = 0; j < stacks; j++) {
      float const phi = 2 * M_PI * j * J;

      float const x = sin (theta) * cos (phi);
      float const y = -cos (theta);
      float const z = sin (phi) * sin (theta);

      *v++ = x * radius;
      *v++ = y * radius;
      *v++ = z * radius;

      *t++ = j * J;
      *t++ = i * I;
    }
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_positions);
  gl->BufferData (GL_ARRAY_BUFFER, sizeof (GLfloat) * vertex_count * 3,
      vertices, GL_STATIC_DRAW);

  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_uv);
  gl->BufferData (GL_ARRAY_BUFFER, sizeof (GLfloat) * vertex_count * 2,
      texcoords, GL_STATIC_DRAW);

  int vau = 0;
  self->index_size = slices * stacks * 2 * 3;
  indices = (GLushort *) malloc (sizeof (GLushort) * self->index_size);

  GLushort *indextemp = indices;
  for (int i = 0; i < slices; i++) {
    for (int j = 0; j < stacks; j++) {
      int next = (j + 1) % stacks;
      *indextemp++ = vau + j;
      *indextemp++ = vau + next;
      *indextemp++ = vau + j + stacks;

      *indextemp++ = vau + next;
      *indextemp++ = vau + next + stacks;
      *indextemp++ = vau + j + stacks;
    }
    vau += stacks;
  }

  // upload index
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, self->vbo_indices);
  gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (GLushort) * self->index_size,
      indices, GL_STATIC_DRAW);

  self->draw_mode = GL_TRIANGLES;
}

void
gst_3d_mesh_draw (Gst3DMesh * self)
{
  GstGLFuncs *gl = self->context->gl_vtable;
  gl->DrawElements (self->draw_mode, self->index_size, GL_UNSIGNED_SHORT, 0);
}


void
gst_3d_mesh_draw_arrays (Gst3DMesh * self)
{
  GstGLFuncs *gl = self->context->gl_vtable;
  gl->DrawArrays (self->draw_mode, 0, self->index_size);
}

void
gst_3d_mesh_bind_to_shader (Gst3DMesh * self, Gst3DShader * shader)
{
  gst_3d_mesh_bind_buffers (self, shader->attr_position, shader->attr_uv);
  gst_3d_shader_enable_attribs (shader);
}


/*
struct Gst3DAttributeBuffer
{
  const gchar *name;
  gint location;
  guint vector_length;
  GLenum element_type;
  guint offset;                 // in bytes
  guint stride;                 // in bytes
};

*/

void
gst_3d_mesh_upload_plane (Gst3DMesh * self, float aspect)
{
  GstGLFuncs *gl = self->context->gl_vtable;

  /* *INDENT-OFF* */
  GLfloat vertices[] = {
     -aspect,  1.0,  0.0, 1.0,
      aspect,  1.0,  0.0, 1.0,
      aspect, -1.0,  0.0, 1.0,
     -aspect, -1.0,  0.0, 1.0
  };
  GLfloat uvs[] = {
     0.0, 1.0,
     1.0, 1.0,
     1.0, 0.0,
     0.0, 0.0
  };
  /* *INDENT-ON* */
  const GLushort indices[] = { 0, 1, 2, 3, 0 };

  self->vertex_count = 4;
  //self->vector_length = 4;

  struct Gst3DAttributeBuffer position_buffer;
  position_buffer.name = "position";
  position_buffer.element_type = GL_FLOAT;
  position_buffer.element_size = sizeof (GLfloat);
  position_buffer.vector_length = 4;

  gl->GenBuffers (1, &position_buffer.location);

  GST_ERROR ("generated position buffer %d", position_buffer.location);

  gl->BindBuffer (GL_ARRAY_BUFFER, position_buffer.location);
  gl->BufferData (GL_ARRAY_BUFFER,
      self->vertex_count * position_buffer.vector_length * sizeof (GLfloat),
      vertices, GL_STATIC_DRAW);


  self->attribute_buffers =
      g_list_append (self->attribute_buffers, &position_buffer);


  // index
  self->index_size = sizeof (indices);

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, self->vbo_indices);
  gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, self->index_size, indices,
      GL_STATIC_DRAW);

  struct Gst3DAttributeBuffer uv_buffer;
  uv_buffer.name = "uv";
  uv_buffer.element_type = GL_FLOAT;
  uv_buffer.element_size = sizeof (GLfloat);
  uv_buffer.vector_length = 2;

  gl->GenBuffers (1, &uv_buffer.location);
  GST_ERROR ("generated uv buffer %d", uv_buffer.location);

  // load uv coords
  gl->BindBuffer (GL_ARRAY_BUFFER, uv_buffer.location);
  gl->BufferData (GL_ARRAY_BUFFER,
      4 * 2 * sizeof (GLfloat), uvs, GL_STATIC_DRAW);
  GST_ERROR ("done loading uv buffer");

  self->attribute_buffers = g_list_append (self->attribute_buffers, &uv_buffer);

  self->draw_mode = GL_TRIANGLE_STRIP;
}


void
gst_3d_mesh_upload_line (Gst3DMesh * self, graphene_vec3_t * from,
    graphene_vec3_t * to, graphene_vec3_t * color)
{
  GstGLFuncs *gl = self->context->gl_vtable;

  /* *INDENT-OFF* */
  GLfloat vertices[] = {
     graphene_vec3_get_x(from), graphene_vec3_get_y(from), graphene_vec3_get_z(from), 1.0,
     graphene_vec3_get_x(to), graphene_vec3_get_y(to), graphene_vec3_get_z(to), 1.0
  };
  GLfloat colors[] = {
     graphene_vec3_get_x(color), graphene_vec3_get_y(color), graphene_vec3_get_z(color),
     graphene_vec3_get_x(color), graphene_vec3_get_y(color), graphene_vec3_get_z(color)
  };
  /* *INDENT-ON* */

  int vertex_count = 2;
  self->vector_length = 4;

  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_positions);
  gl->BufferData (GL_ARRAY_BUFFER,
      vertex_count * 4 * sizeof (GLfloat), vertices, GL_STATIC_DRAW);

  // load colors
  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_color);
  gl->BufferData (GL_ARRAY_BUFFER,
      vertex_count * 3 * sizeof (GLfloat), colors, GL_STATIC_DRAW);

  self->draw_mode = GL_LINES;
}


void
gst_3d_mesh_upload_point_plane (Gst3DMesh * self, unsigned width,
    unsigned height)
{
  GLfloat *vertices;
  GLfloat *texcoords;
  GLuint *indices;

  GstGLFuncs *gl = self->context->gl_vtable;

  self->vector_length = 3;
  int vertex_count = width * height;
  const int component_size = sizeof (GLfloat) * vertex_count;

  vertices = (GLfloat *) malloc (component_size * 3);
  texcoords = (GLfloat *) malloc (component_size * 2);

  GLfloat *v = vertices;
  GLfloat *t = texcoords;

  float w_step = 2.0 / (float) width;
  float h_step = 2.0 / (float) height;

  float curent_w = -1.0;

  float u_step = 1.0 / (float) width;
  float v_step = 1.0 / (float) height;

  float curent_u = 0.0;

  for (int i = 0; i < width; i++) {
    float curent_h = -1.0;
    float curent_v = 0.0;
    for (int j = 0; j < height; j++) {

      *v++ = curent_w;
      *v++ = curent_h;
      *v++ = 0;

      *t++ = curent_u;
      *t++ = curent_v;

      curent_h += h_step;
      curent_v += v_step;
    }
    curent_u += u_step;
    curent_w += w_step;
  }

  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_positions);
  gl->BufferData (GL_ARRAY_BUFFER, sizeof (GLfloat) * vertex_count * 3,
      vertices, GL_STATIC_DRAW);
/*
  gl->BindBuffer (GL_ARRAY_BUFFER, self->vbo_uv);
  gl->BufferData (GL_ARRAY_BUFFER, sizeof (GLfloat) * vertex_count * 2,
      texcoords, GL_STATIC_DRAW);
*/
  self->index_size = width * height;
  indices = (GLuint *) malloc (sizeof (GLuint) * self->index_size);

  GLuint *indextemp = indices;
  for (int i = 0; i < self->index_size; i++) {
    *indextemp++ = i;
  }

  // upload index
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, self->vbo_indices);
  gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (GLuint) * self->index_size,
      indices, GL_STATIC_DRAW);
  self->draw_mode = GL_POINTS;
}
