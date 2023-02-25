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

#ifndef _GST_NMEASOURCE_H_
#define _GST_NMEASOURCE_H_

#include <gst/base/gstpushsrc.h>
#include "nmealoop.h"

#include "../gstreamPipeline.h"


G_BEGIN_DECLS

#define GST_TYPE_NMEASOURCE   (gst_nmeasource_get_type())
#define GST_NMEASOURCE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NMEASOURCE,GstNmeaSource))
#define GST_NMEASOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NMEASOURCE,GstNmeaSourceClass))
#define GST_IS_NMEASOURCE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NMEASOURCE))
#define GST_IS_NMEASOURCE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NMEASOURCE))

typedef struct _GstNmeaSource GstNmeaSource;
typedef struct _GstNmeaSourceClass GstNmeaSourceClass;


struct _GstNmeaSource
{
  GstPushSrcClass base_nmeasource;

  struct nmea_threadInfo threadInfo;

  GstClockID waitId;

  gstreamPipeline *parent;

};

struct _GstNmeaSourceClass
{
  GstPushSrcClass base_nmeasource_class;
};

GType gst_nmeasource_get_type (void);

void gst_nmeasource_registerRunTimePlugin();

G_END_DECLS

#endif
