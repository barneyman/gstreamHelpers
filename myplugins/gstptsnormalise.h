/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2020 Niels De Graef <niels.degraef@gmail.com>
 * Copyright (C) 2021 pi <<user@hostname.org>>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef __GST_PTSNORMALISE_H__
#define __GST_PTSNORMALISE_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_PTSNORMALISE (gst_ptsnormalise_get_type())
G_DECLARE_FINAL_TYPE (Gstptsnormalise, gst_ptsnormalise,
    GST, PTSNORMALISE, GstBaseTransform)

struct _Gstptsnormalise {
  GstBaseTransform element;

  gboolean silent;
  int clip_ms;

  GstClockTime basetime_first_packet, segment_start;

  GstSegment clippedSegment;

  GstClockTime lastSeenPts;
};

void ptsnormalise_registerRunTimePlugin();

G_END_DECLS

#endif /* __GST_PTSNORMALISE_H__ */
