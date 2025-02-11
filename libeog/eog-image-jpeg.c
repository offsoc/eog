/* This code is based on the jpeg saving code from gdk-pixbuf. Full copyright 
 * notice is given in the following:
 */
/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Michael Zucchi
 * Copyright (C) 1999 The Free Software Foundation
 * 
 * Progressive loading code Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Michael Fulbright <drmike@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-image-jpeg.h"

#if HAVE_JPEG

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>
#include <transupp.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif
#include "eog-image-private.h"

#ifdef G_OS_WIN32
#define sigjmp_buf jmp_buf
#define sigsetjmp(env, savesigs) setjmp (env)
#define siglongjmp longjmp
#endif

typedef enum {
	EOG_SAVE_NONE,
	EOG_SAVE_JPEG_AS_JPEG,
	EOG_SAVE_ANY_AS_JPEG
} EogJpegSaveMethod;

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
        GError **error;
	const char *filename;
};


static void
fatal_error_handler (j_common_ptr cinfo)
{
	struct error_handler_data *errmgr;
        char buffer[JMSG_LENGTH_MAX];
        
	errmgr = (struct error_handler_data *) cinfo->err;
        
        /* Create the message */
        (* cinfo->err->format_message) (cinfo, buffer);

        /* broken check for *error == NULL for robustness against
         * crappy JPEG library
         */
        if (errmgr->error && *errmgr->error == NULL) {
                g_set_error (errmgr->error,
			     0,
			     0,
			     "Error interpreting JPEG image file: %s\n\n%s",
			     g_path_get_basename (errmgr->filename),
                             buffer);
        }

	siglongjmp (errmgr->setjmp_buffer, 1);

        g_assert_not_reached ();
}


static void
output_message_handler (j_common_ptr cinfo)
{
	/* This method keeps libjpeg from dumping crap to stderr */
	/* do nothing */
}

static void
init_transform_info (EogImage *image, jpeg_transform_info *info)
{
	JXFORM_CODE trans_code = JXFORM_NONE;
	EogTransformType transformation;

	g_return_if_fail (EOG_IS_IMAGE (image));

	if (image->priv->trans != NULL) {
		transformation = eog_transform_get_transform_type (image->priv->trans);
		switch (transformation) {
		case EOG_TRANSFORM_ROT_90:
			trans_code = JXFORM_ROT_90; 
			break;
		case EOG_TRANSFORM_ROT_270:
			trans_code = JXFORM_ROT_270; 
			break;
		case EOG_TRANSFORM_ROT_180:
			trans_code = JXFORM_ROT_180; 
			break;
		case EOG_TRANSFORM_FLIP_HORIZONTAL:
			trans_code = JXFORM_FLIP_H;
			break;
		case EOG_TRANSFORM_FLIP_VERTICAL:
			trans_code = JXFORM_FLIP_V;
			break;
		default:
			trans_code = JXFORM_NONE;
			break;
		}
	}

	info->transform = trans_code;
	info->trim      = FALSE;
	info->force_grayscale = FALSE;
}


static gboolean
_save_jpeg_as_jpeg (EogImage *image, const char *file, EogImageSaveInfo *source, 
		    EogImageSaveInfo *target, GError **error)
{
	struct jpeg_decompress_struct  srcinfo;
	struct jpeg_compress_struct    dstinfo;
	struct error_handler_data      jsrcerr, jdsterr;
	jpeg_transform_info            transformoption; 
	jvirt_barray_ptr              *src_coef_arrays;
	jvirt_barray_ptr              *dst_coef_arrays;
	FILE                          *output_file;
	FILE                          *input_file;
	EogImagePrivate               *priv;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (EOG_IMAGE (image)->priv->uri != NULL, FALSE);

	priv = image->priv;

	init_transform_info (image, &transformoption);
	
	/* Initialize the JPEG decompression object with default error 
	 * handling. */
	jsrcerr.filename = gnome_vfs_uri_get_path (priv->uri);
	srcinfo.err = jpeg_std_error (&(jsrcerr.pub));
	jsrcerr.pub.error_exit = fatal_error_handler;
	jsrcerr.pub.output_message = output_message_handler;
	jsrcerr.error = error;

	jpeg_create_decompress (&srcinfo);

	/* Initialize the JPEG compression object with default error 
	 * handling. */
	jdsterr.filename = file;
	dstinfo.err = jpeg_std_error (&(jdsterr.pub));
	jdsterr.pub.error_exit = fatal_error_handler;
	jdsterr.pub.output_message = output_message_handler;
	jdsterr.error = error;

	jpeg_create_compress (&dstinfo);
	
	dstinfo.err->trace_level = 0;
	dstinfo.arith_code = FALSE;
	dstinfo.optimize_coding = FALSE;

	jsrcerr.pub.trace_level = jdsterr.pub.trace_level;
	srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

	/* Open the output file. */
	/* FIXME: Make this a GnomeVFSURI aware input manager */
	input_file = fopen (gnome_vfs_uri_get_path (priv->uri), "rb");
	if (input_file == NULL) {
		g_warning ("Input file not openable: %s\n", gnome_vfs_uri_get_path (priv->uri));
		return FALSE;
	}

	output_file = fopen (file, "wb");
	if (output_file == NULL) {
		g_warning ("Output file not openable: %s\n", file);
		fclose (input_file);
		return FALSE;
	}

	if (sigsetjmp (jsrcerr.setjmp_buffer, 1)) {
		fclose (output_file);
		fclose (input_file);
		jpeg_destroy_compress (&dstinfo);
		jpeg_destroy_decompress (&srcinfo);
		return FALSE;
	}

	if (sigsetjmp (jdsterr.setjmp_buffer, 1)) {
		fclose (output_file);
		fclose (input_file);
		jpeg_destroy_compress (&dstinfo);
		jpeg_destroy_decompress (&srcinfo);
		return FALSE;
	}

	/* Specify data source for decompression */
	jpeg_stdio_src (&srcinfo, input_file);

	/* Enable saving of extra markers that we want to copy */
	jcopy_markers_setup (&srcinfo, JCOPYOPT_DEFAULT);

	/* Read file header */
	(void) jpeg_read_header (&srcinfo, TRUE);

	/* Any space needed by a transform option must be requested before
	 * jpeg_read_coefficients so that memory allocation will be done right.
	 */
	jtransform_request_workspace (&srcinfo, &transformoption);

	/* Read source file as DCT coefficients */
	src_coef_arrays = jpeg_read_coefficients (&srcinfo);

	/* Initialize destination compression parameters from source values */
	jpeg_copy_critical_parameters (&srcinfo, &dstinfo);

	/* Adjust destination parameters if required by transform options;
	 * also find out which set of coefficient arrays will hold the output.
	 */
	dst_coef_arrays = jtransform_adjust_parameters (&srcinfo, 
							&dstinfo,
							src_coef_arrays,
							&transformoption);

	/* Specify data destination for compression */
	jpeg_stdio_dest (&dstinfo, output_file);

	/* Start compressor (note no image data is actually written here) */
	jpeg_write_coefficients (&dstinfo, dst_coef_arrays);

	/* handle EXIF/IPTC data explicitly */
#if HAVE_EXIF
	/* exif_chunk and exif are mutally exclusvie, this is what we assure here */
	g_assert (priv->exif_chunk == NULL);
	if (priv->exif != NULL)
	{
		unsigned char *exif_buf;
		unsigned int   exif_buf_len;

		g_print ("save exif data\n");
		exif_data_save_data (priv->exif, &exif_buf, &exif_buf_len);
		jpeg_write_marker (&dstinfo, JPEG_APP0+1, exif_buf, exif_buf_len);
		g_free (exif_buf);
	}
#else
	if (priv->exif_chunk != NULL) {
		g_print ("save exif raw data\n");
		jpeg_write_marker (&dstinfo, JPEG_APP0+1, priv->exif_chunk, priv->exif_chunk_len);
	}
#endif
	/* FIXME: Consider IPTC data too */

	/* Copy to the output file any extra markers that we want to 
	 * preserve */
	jcopy_markers_execute (&srcinfo, &dstinfo, JCOPYOPT_DEFAULT);

	/* Execute image transformation, if any */
	jtransform_execute_transformation (&srcinfo, 
					   &dstinfo,
					   src_coef_arrays,
					   &transformoption);

	/* Finish compression and release memory */
	jpeg_finish_compress (&dstinfo);
	jpeg_destroy_compress (&dstinfo);
	(void) jpeg_finish_decompress (&srcinfo);
	jpeg_destroy_decompress (&srcinfo);

	/* Close files */
	fclose (input_file);
	fclose (output_file);

	return TRUE;	
}

static gboolean
_save_any_as_jpeg (EogImage *image, const char *file, EogImageSaveInfo *source, 
		   EogImageSaveInfo *target, GError **error)
{
	EogImagePrivate *priv;
	GdkPixbuf *pixbuf;
	struct jpeg_compress_struct cinfo;
	guchar *buf = NULL;
	guchar *ptr;
	guchar *pixels = NULL;
	JSAMPROW *jbuf;
	int y = 0;
	volatile int quality = 75; /* default; must be between 0 and 100 */
	int i, j;
	int w, h = 0;
	int rowstride = 0;
	FILE *outfile;
	struct error_handler_data jerr;
	
	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (EOG_IMAGE (image)->priv->image != NULL, FALSE);
	
	priv = image->priv;
	pixbuf = priv->image;
	
	outfile = fopen (file, "wb");
	if (outfile == NULL) {
		g_set_error (error,             /* FIXME: Better error message */
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Couldn't create temporary file for saving: %s"),
			     file);
		return FALSE;
	}

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	
	/* no image data? abort */
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	g_return_val_if_fail (pixels != NULL, FALSE);
	
	/* allocate a small buffer to convert image data */
	buf = g_try_malloc (w * 3 * sizeof (guchar));
	if (!buf) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Couldn't allocate memory for loading JPEG file"));
		return FALSE;
	}
	
	/* set up error handling */	
	jerr.filename = file;
	cinfo.err = jpeg_std_error (&(jerr.pub));
	jerr.pub.error_exit = fatal_error_handler;
	jerr.pub.output_message = output_message_handler;
	jerr.error = error;

	/* setup compress params */
	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, outfile);
	cinfo.image_width      = w;
	cinfo.image_height     = h;
	cinfo.input_components = 3; 
	cinfo.in_color_space   = JCS_RGB;

	/* error exit routine */
	if (sigsetjmp (jerr.setjmp_buffer, 1)) {
		fclose (outfile);
		jpeg_destroy_compress (&cinfo);
		return FALSE;
	}

	/* set desired jpeg quality if available */
	if (target != NULL && target->jpeg_quality >= 0.0) {
		quality = (int) MIN (target->jpeg_quality, 1.0) * 100;
	}
	
	/* set up jepg compression parameters */
	jpeg_set_defaults (&cinfo);
	jpeg_set_quality (&cinfo, quality, TRUE);
	jpeg_start_compress (&cinfo, TRUE);
	
	/* write EXIF/IPTC data explicitly */
#if HAVE_EXIF
	/* exif_chunk and exif are mutally exclusvie, this is what we assure here */
	g_assert (priv->exif_chunk == NULL);
	if (priv->exif != NULL)
	{
		unsigned char *exif_buf;
		unsigned int   exif_buf_len;
		
		g_print ("save exif data\n");
		exif_data_save_data (priv->exif, &exif_buf, &exif_buf_len);
		jpeg_write_marker (&cinfo, 0xe1, exif_buf, exif_buf_len);
		g_free (exif_buf);
	}
#else
	if (priv->exif_chunk != NULL) {
		g_print ("save exif raw data\n");
		jpeg_write_marker (&cinfo, JPEG_APP0+1, priv->exif_chunk, priv->exif_chunk_len);
	}
#endif
	/* FIXME: Consider IPTC data too */

	/* get the start pointer */
	ptr = pixels;
	/* go one scanline at a time... and save */
	i = 0;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* convert scanline from ARGB to RGB packed */
		for (j = 0; j < w; j++)
			memcpy (&(buf[j*3]), &(ptr[i*rowstride + j*3]), 3);
		
		/* write scanline */
		jbuf = (JSAMPROW *)(&buf);
		jpeg_write_scanlines (&cinfo, jbuf, 1);
		i++;
		y++;
		
	}
	
	/* finish off */
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress(&cinfo);
	g_free (buf);

	fclose (outfile);

	return TRUE;
}

gboolean 
eog_image_jpeg_save_file (EogImage *image, const char *file, 
			  EogImageSaveInfo *source, EogImageSaveInfo *target,
			  GError **error)
{
	EogJpegSaveMethod method = EOG_SAVE_NONE;
	gboolean source_is_jpeg = FALSE;
	gboolean target_is_jpeg = FALSE;
        gboolean result;

	g_return_val_if_fail (source != NULL, FALSE);

	source_is_jpeg = !g_ascii_strcasecmp (source->format, EOG_FILE_FORMAT_JPEG);

	/* determine which method should be used for saving */
	if (target == NULL) {
		if (source_is_jpeg) {
			method = EOG_SAVE_JPEG_AS_JPEG;
		}
	}
	else {
		target_is_jpeg = !g_ascii_strcasecmp (target->format, EOG_FILE_FORMAT_JPEG);

		if (source_is_jpeg && target_is_jpeg) {
			if (target->jpeg_quality < 0.0) {
				method = EOG_SAVE_JPEG_AS_JPEG;
			}
			else {
				/* reencoding is required, cause quality is set */
				method = EOG_SAVE_ANY_AS_JPEG;
			}
		}
		else if (!source_is_jpeg && target_is_jpeg) {
			method = EOG_SAVE_ANY_AS_JPEG;
		}
	}

	switch (method) {
	case EOG_SAVE_JPEG_AS_JPEG:
		result = _save_jpeg_as_jpeg (image, file, source, target, error);
		break;
	case EOG_SAVE_ANY_AS_JPEG:
		result = _save_any_as_jpeg (image, file, source, target, error);
		break;
	default:
		result = FALSE;
	}

	return result;
}


/* ===================================================================

                       deprecated stuff 

   ------------------------------------------------------------------*/

static char*
get_tmp_filepath (void)
{
	char *tmp_file;
	char *tmp_file_name;
	static int file_count = 0;

	tmp_file_name = g_strdup_printf ("eog%i%i.tmp", getpid (), file_count++);
	tmp_file = g_build_filename (g_get_tmp_dir (), tmp_file_name, NULL);

	g_free (tmp_file_name);

	return tmp_file;
}

static GnomeVFSResult
move_file_to_uri (char *file_path, GnomeVFSURI *uri)
{
	GnomeVFSResult result;
	GnomeVFSURI *source_uri;

	source_uri = gnome_vfs_uri_new (file_path);
		
	result = gnome_vfs_xfer_uri (source_uri,
				     uri, 
				     GNOME_VFS_XFER_DELETE_ITEMS,           /* delete source file */
				     GNOME_VFS_XFER_ERROR_MODE_ABORT,       /* abort on all errors */
				     GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, /* we checked for existing 
									       file already */
				     NULL,                                  /* no progress callback */
				     NULL);

	gnome_vfs_uri_unref (source_uri);

	return result;
}	


gboolean
eog_image_jpeg_save (EogImage *image, GnomeVFSURI *uri, GError **error)
{
	EogImagePrivate *priv;
	GdkPixbuf *pixbuf;
	GnomeVFSResult vfs_result;
	char *tmp_path;
	struct jpeg_compress_struct cinfo;
	guchar *buf = NULL;
	guchar *ptr;
	guchar *pixels = NULL;
	JSAMPROW *jbuf;
	int y = 0;
	volatile int quality = 75; /* default; must be between 0 and 100 */
	int i, j;
	int w, h = 0;
	int rowstride = 0;
	FILE *outfile;
	struct jpeg_error_mgr jerr;
#if HAVE_EXIF
	unsigned char *exif_buf;
	unsigned int   exif_buf_len;
#endif
	
	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	
	priv = image->priv;

	pixbuf = priv->image;
	if (pixbuf == NULL) {
		return FALSE;
	}
	
	tmp_path = get_tmp_filepath ();
	outfile = fopen (tmp_path, "wb");
	if (outfile == NULL) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Couldn't create temporary file for saving: %s"),
			     tmp_path);
		g_free (tmp_path);
		return FALSE;
	}

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	
	/* no image data? abort */
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	g_return_val_if_fail (pixels != NULL, FALSE);
	
	/* allocate a small buffer to convert image data */
	buf = g_try_malloc (w * 3 * sizeof (guchar));
	if (!buf) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Couldn't allocate memory for loading JPEG file"));
		return FALSE;
	}
	
	/* set up error handling */
	
	cinfo.err = jpeg_std_error (&jerr);
	
	/* setup compress params */
	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, outfile);
	cinfo.image_width      = w;
	cinfo.image_height     = h;
	cinfo.input_components = 3; 
	cinfo.in_color_space   = JCS_RGB;
	
	/* set up jepg compression parameters */
	jpeg_set_defaults (&cinfo);
	jpeg_set_quality (&cinfo, quality, TRUE);
	jpeg_start_compress (&cinfo, TRUE);
	
#if HAVE_EXIF
	if (priv->exif != NULL)
	{
		g_print ("save exif data\n");
		exif_data_save_data (priv->exif, &exif_buf, &exif_buf_len);
		jpeg_write_marker (&cinfo, 0xe1, exif_buf, exif_buf_len);
		g_free (exif_buf);
	}
#endif
	/* get the start pointer */
	ptr = pixels;
	/* go one scanline at a time... and save */
	i = 0;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* convert scanline from ARGB to RGB packed */
		for (j = 0; j < w; j++)
			memcpy (&(buf[j*3]), &(ptr[i*rowstride + j*3]), 3);
		
		/* write scanline */
		jbuf = (JSAMPROW *)(&buf);
		jpeg_write_scanlines (&cinfo, jbuf, 1);
		i++;
		y++;
		
	}
	
	/* finish off */
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress(&cinfo);
	g_free (buf);

	fclose (outfile);

	/* move temporary file to final destination */
	vfs_result = move_file_to_uri (tmp_path, uri);
	if (vfs_result != GNOME_VFS_OK) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_VFS, 
			     gnome_vfs_result_to_string (vfs_result));
	}
	g_free (tmp_path);

	return (vfs_result == GNOME_VFS_OK);
}

gboolean 
eog_image_jpeg_save_lossless (EogImage *image, GnomeVFSURI *uri, GError **error)
{
	char *img_path;
	char *img_uri_txt;
	int result;
	JXFORM_CODE trans_code = JXFORM_NONE;
	EogTransformType transformation;
	char *tmp_file;
	GnomeVFSResult vfs_result = GNOME_VFS_OK;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	img_uri_txt = gnome_vfs_uri_to_string (image->priv->uri, GNOME_VFS_URI_HIDE_NONE);
	img_path = gnome_vfs_get_local_path_from_uri (img_uri_txt);

	if (image->priv->trans != NULL) {
		transformation = eog_transform_get_transform_type (image->priv->trans);
		switch (transformation) {
		case EOG_TRANSFORM_ROT_90:
			trans_code = JXFORM_ROT_90; 
			break;
		case EOG_TRANSFORM_ROT_270:
			trans_code = JXFORM_ROT_270; 
			break;
		case EOG_TRANSFORM_ROT_180:
			trans_code = JXFORM_ROT_180; 
			break;
		case EOG_TRANSFORM_FLIP_HORIZONTAL:
			trans_code = JXFORM_FLIP_H;
			break;
		case EOG_TRANSFORM_FLIP_VERTICAL:
			trans_code = JXFORM_FLIP_V;
			break;
		default:
			trans_code = JXFORM_NONE;
			break;
		}
	}

	/* We save the resulting jpeg file to a temporary location
	 * first and move it to the final destination then. This way
	 * we can also overwrite a file.
	 */
	tmp_file = get_tmp_filepath ();

	result = FALSE; /* jpegtran ((char*) img_path, (char*) tmp_file, trans_code, error); */

	if (result == 0) {
		/* image successfully written */
		vfs_result = move_file_to_uri (tmp_file, uri);
		if (vfs_result != GNOME_VFS_OK) {
			g_set_error (error, EOG_IMAGE_ERROR,
				     EOG_IMAGE_ERROR_VFS, 
				     gnome_vfs_result_to_string (vfs_result));
		}
	}

	if (g_file_test (tmp_file, G_FILE_TEST_EXISTS)) {
		gnome_vfs_unlink (tmp_file);
	}

	g_free (tmp_file);
	g_free (img_path);
	g_free (img_uri_txt);

	return ((result == 0) && (vfs_result == GNOME_VFS_OK));
}

#endif
