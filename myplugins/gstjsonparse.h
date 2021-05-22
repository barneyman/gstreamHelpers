/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
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

#ifndef _GST_JSONPARSE_H_
#define _GST_JSONPARSE_H_

#include <gst/base/gstbasetransform.h>
#include "../gstreamBin.h"

G_BEGIN_DECLS

#define GST_TYPE_JSONPARSE   (gst_jsonparse_get_type())
#define GST_JSONPARSE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JSONPARSE,GstJsonparse))
#define GST_JSONPARSE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JSONPARSE,GstJsonparseClass))
#define GST_IS_JSONPARSE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JSONPARSE))
#define GST_IS_JSONPARSE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JSONPARSE))

typedef struct _GstJsonparse GstJsonparse;
typedef struct _GstJsonparseClass GstJsonparseClass;

struct _GstJsonparse
{
  GstBaseTransform base_jsonparse;

  // private, actually a pointer to me GstBin class
  gstreamBin *myClassPointer;

};

struct _GstJsonparseClass
{
  GstBaseTransformClass base_jsonparse_class;
};

GType gst_jsonparse_get_type (void);

void jsonparse_registerRunTimePlugin();


G_END_DECLS

#endif
