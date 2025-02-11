#ifndef _EOG_IMAGE_JPEG_H_
#define _EOG_IMAGE_JPEG_H_

#if HAVE_JPEG

#include <glib.h>
#include "eog-image.h"
#include "eog-image-save-info.h"

/* Saves a source jpeg file in an arbitrary format (as specified by
 * target). The target pointer may be NULL, in which case the output
 * file is saved as jpeg too.  This method tries to be as smart as
 * possible. It will save the image as lossless as possible (if the
 * target is a jpeg image too).
 */
gboolean eog_image_jpeg_save_file (EogImage *image, const char *file, 
				   EogImageSaveInfo *source, EogImageSaveInfo *target,
				   GError **error);


/* =======================

         deprecated 

   ---------------------*/
gboolean eog_image_jpeg_save (EogImage *image, GnomeVFSURI *uri, GError **error);
gboolean eog_image_jpeg_save_lossless (EogImage *image, GnomeVFSURI *uri, GError **error);

#endif

#endif /* _EOG_IMAGE_JPEG_H_ */
