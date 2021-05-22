#ifndef _GST_MYBIN_H_
#define _GST_MYBIN_H_

#include <gst/gstbin.h>
#include "../gstreamBin.h"


G_BEGIN_DECLS

#define GST_TYPE_MYBIN   (gst_mybin_get_type())
#define GST_MYBIN(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MYBIN,GstMyBin))
#define GST_MYBIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MYBIN,GstMyBinClass))
#define GST_IS_MYBIN(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MYBIN))
#define GST_IS_MYBIN_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MYBIN))

typedef struct _GstMyBin GstMyBin;
typedef struct _GstMyBinClass GstMyBinClass;

struct _GstMyBin
{
  GstBin base_mybin;

  // private, actually a pointer to me GstBin class
  gstreamBin *myClassPointer;



};

struct _GstMyBinClass
{
  GstBinClass base_mybin_class;
};

GType gst_mybin_get_type (void);

void gst_mybin_registerRunTimePlugin();

G_END_DECLS

#endif
