#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-macros.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "eog-info-view-exif.h"

#if HAVE_EXIF

#include <libexif/exif-entry.h>
#include <libexif/exif-utils.h>

typedef enum {
	EXIF_CATEGORY_CAMERA,
	EXIF_CATEGORY_IMAGE_DATA,
	EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS,
	EXIF_CATEGORY_MAKER_NOTE,
	EXIF_CATEGORY_OTHER
} ExifCategory;

typedef struct {
	char *label;
	char *path;
} ExifCategoryInfo;

static ExifCategoryInfo exif_categories[] = {
	{ N_("Camera"),                  "0" },
	{ N_("Image Data"),              "1" },
	{ N_("Image Taking Conditions"), "2" },
	{ N_("Maker Note"),              "3" },
	{ N_("Other"),                   "4" },
	{ NULL, NULL }
};

typedef struct {
	int id;
	ExifCategory category;
} ExifTagCategory;

static ExifTagCategory exif_tag_category_map[] = {
	{ EXIF_TAG_INTEROPERABILITY_INDEX,    EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_INTEROPERABILITY_VERSION,  EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_IMAGE_WIDTH,               EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_IMAGE_LENGTH        ,      EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_BITS_PER_SAMPLE 	     ,EXIF_CATEGORY_CAMERA },
	{ EXIF_TAG_COMPRESSION 		    , EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_PHOTOMETRIC_INTERPRETATION 	, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_FILL_ORDER 		, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_DOCUMENT_NAME 	, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_IMAGE_DESCRIPTION 	, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_MAKE 		, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_MODEL 		, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_STRIP_OFFSETS 	, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_ORIENTATION 		, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_SAMPLES_PER_PIXEL 	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_ROWS_PER_STRIP 	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_STRIP_BYTE_COUNTS	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_X_RESOLUTION 	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_Y_RESOLUTION 	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_PLANAR_CONFIGURATION , EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_RESOLUTION_UNIT 	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_TRANSFER_FUNCTION 	, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_SOFTWARE 		, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_DATE_TIME		, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_ARTIST		, EXIF_CATEGORY_IMAGE_DATA},
	{ EXIF_TAG_WHITE_POINT		, 	EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_PRIMARY_CHROMATICITIES, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_TRANSFER_RANGE	, EXIF_CATEGORY_OTHER}, 
	{ EXIF_TAG_JPEG_PROC		, EXIF_CATEGORY_OTHER}, 
	{ EXIF_TAG_JPEG_INTERCHANGE_FORMAT, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH, },
	{ EXIF_TAG_YCBCR_COEFFICIENTS	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_YCBCR_SUB_SAMPLING	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_YCBCR_POSITIONING	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_REFERENCE_BLACK_WHITE, 	EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_RELATED_IMAGE_FILE_FORMAT,EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_RELATED_IMAGE_WIDTH	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_RELATED_IMAGE_LENGTH	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_CFA_REPEAT_PATTERN_DIM, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_CFA_PATTERN		, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_BATTERY_LEVEL	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_COPYRIGHT		, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_EXPOSURE_TIME	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_FNUMBER		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_IPTC_NAA		, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_EXIF_IFD_POINTER	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_INTER_COLOR_PROFILE	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_EXPOSURE_PROGRAM	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_SPECTRAL_SENSITIVITY	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_GPS_INFO_IFD_POINTER	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_ISO_SPEED_RATINGS	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_OECF			, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_EXIF_VERSION		, EXIF_CATEGORY_CAMERA},	
	{ EXIF_TAG_DATE_TIME_ORIGINAL	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_DATE_TIME_DIGITIZED	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_COMPONENTS_CONFIGURATION	, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_COMPRESSED_BITS_PER_PIXEL, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_SHUTTER_SPEED_VALUE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_APERTURE_VALUE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_BRIGHTNESS_VALUE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_EXPOSURE_BIAS_VALUE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_MAX_APERTURE_VALUE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_SUBJECT_DISTANCE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_METERING_MODE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_LIGHT_SOURCE		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_FLASH		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_FOCAL_LENGTH		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_SUBJECT_AREA		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_MAKER_NOTE		, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_USER_COMMENT		, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_SUBSEC_TIME		, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_SUB_SEC_TIME_ORIGINAL, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_SUB_SEC_TIME_DIGITIZED, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_FLASH_PIX_VERSION	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_COLOR_SPACE		, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_PIXEL_X_DIMENSION	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_PIXEL_Y_DIMENSION	, EXIF_CATEGORY_IMAGE_DATA},	
	{ EXIF_TAG_RELATED_SOUND_FILE	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_INTEROPERABILITY_IFD_POINTER, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_FLASH_ENERGY		,EXIF_CATEGORY_OTHER },	
	{ EXIF_TAG_SPATIAL_FREQUENCY_RESPONSE, EXIF_CATEGORY_OTHER},
	{ EXIF_TAG_FOCAL_PLANE_X_RESOLUTION, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_SUBJECT_LOCATION, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_EXPOSURE_INDEX, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_SENSING_METHOD, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_FILE_SOURCE	, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_SCENE_TYPE, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_NEW_CFA_PATTERN, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_CUSTOM_RENDERED, EXIF_CATEGORY_OTHER},	
	{ EXIF_TAG_EXPOSURE_MODE, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_WHITE_BALANCE, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},	
	{ EXIF_TAG_DIGITAL_ZOOM_RATIO, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_SCENE_CAPTURE_TYPE	, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_GAIN_CONTROL		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_CONTRAST		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_SATURATION		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_SHARPNESS		, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},
	{ EXIF_TAG_DEVICE_SETTING_DESCRIPTION, EXIF_CATEGORY_CAMERA},
	{ EXIF_TAG_SUBJECT_DISTANCE_RANGE, EXIF_CATEGORY_IMAGE_TAKING_CONDITIONS},		
	{ EXIF_TAG_IMAGE_UNIQUE_ID	, EXIF_CATEGORY_IMAGE_DATA},
	{ -1, -1 }
};

#define MODEL_COLUMN_ATTRIBUTE 0
#define MODEL_COLUMN_VALUE     1

struct _EogInfoViewExifPrivate
{
	GtkTreeModel *model;

	GHashTable   *id_path_hash; /* saves the stringified GtkTreeModel path  
				       for a given EXIF entry id */
};


GNOME_CLASS_BOILERPLATE (EogInfoViewExif,
			 eog_info_view_exif,
			 EogInfoViewDetail,
			 EOG_TYPE_INFO_VIEW_DETAIL);

static char*  set_row_data (GtkTreeStore *store, char *path, char *parent, const char *attribute, const char *value);


static void
eog_info_view_exif_finalize (GObject *object)
{
	EogInfoViewExif *instance = EOG_INFO_VIEW_EXIF (object);
	
	g_free (instance->priv);
	instance->priv = NULL;
}

static void
eog_info_view_exif_dispose (GObject *object)
{
	EogInfoViewExif *view;
	EogInfoViewExifPrivate *priv;

	view = EOG_INFO_VIEW_EXIF (object);
	priv = view->priv;

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->id_path_hash) {
		g_hash_table_destroy (priv->id_path_hash);
		priv->id_path_hash = NULL;
	}
}

static void
eog_info_view_exif_instance_init (EogInfoViewExif *view)
{
	EogInfoViewExifPrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int i;

	priv = g_new0 (EogInfoViewExifPrivate, 1);

	view->priv = priv;

	priv->model = GTK_TREE_MODEL (gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING));
	priv->id_path_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	/* tag column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Tag"),
		                                                   cell, 
                                                           "text", MODEL_COLUMN_ATTRIBUTE,
														   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* value column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Value"),
		                                                   cell, 
                                                           "text", MODEL_COLUMN_VALUE,
							                               NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);

	for (i = 0; exif_categories [i].label != NULL; i++) {
		char *translated_string;
		
		translated_string = gettext (exif_categories[i].label);
		set_row_data (GTK_TREE_STORE (priv->model), exif_categories[i].path, NULL,
					  translated_string, NULL);
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (priv->model));
}

static void 
eog_info_view_exif_class_init (EogInfoViewExifClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_info_view_exif_finalize;
	object_class->dispose = eog_info_view_exif_dispose;
}

static ExifCategory
get_exif_category (ExifEntry *entry)
{
	ExifCategory cat = EXIF_CATEGORY_OTHER;
	int i;

	for (i = 0; exif_tag_category_map [i].id != -1; i++) {
		if (exif_tag_category_map[i].id == (int) entry->tag) {
			cat = exif_tag_category_map[i].category;
			break;
		}
	}

	return cat;
}

static char*
set_row_data (GtkTreeStore *store, char *path, char *parent, const char *attribute, const char *value)
{
	GtkTreeIter iter;
	char *utf_attribute = NULL;
	char *utf_value = NULL;
	gboolean iter_valid = FALSE;

	if (!attribute) return NULL;

	if (path != NULL) {
		iter_valid = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path);
	}

	if (!iter_valid) {
		GtkTreePath *tree_path;
		GtkTreeIter parent_iter;
		gboolean parent_valid = FALSE;

		if (parent != NULL) {
			parent_valid = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &parent_iter, parent);
		}

		gtk_tree_store_append (store, &iter, parent_valid ? &parent_iter : NULL);

		if (path == NULL) {
			tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			
			if (tree_path != NULL) {
				path = gtk_tree_path_to_string (tree_path);
				gtk_tree_path_free (tree_path);
			}
		}
	}

	if (g_utf8_validate (attribute, -1, NULL)) {
		utf_attribute = g_strdup (attribute);
	}
	else {
		utf_attribute = g_locale_to_utf8 (attribute, -1, NULL, NULL, NULL);
	}
	gtk_tree_store_set (store, &iter, MODEL_COLUMN_ATTRIBUTE, utf_attribute, -1);
	g_free (utf_attribute);

	if (value != NULL) {
		if (g_utf8_validate (value, -1, NULL)) {
			utf_value = g_strdup (value);
		}
		else {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		}
		
		gtk_tree_store_set (store, &iter, MODEL_COLUMN_VALUE, utf_value, -1);
		g_free (utf_value);
	}

	return path;
}

static void 
exif_entry_cb (ExifEntry *entry, gpointer data)
{
	GtkTreeStore *store;
	EogInfoViewExif *view;
	EogInfoViewExifPrivate *priv;
	ExifCategory cat;
	char *path;
	char b[1024];

	view = EOG_INFO_VIEW_EXIF (data);
	priv = view->priv;

	store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	
	path = g_hash_table_lookup (priv->id_path_hash, GINT_TO_POINTER (entry->tag));

	if (path != NULL) {
		set_row_data (store, path, NULL, exif_tag_get_name (entry->tag), exif_entry_get_value (entry, b, sizeof(b)));	
	}
	else {
		cat = get_exif_category (entry);
		path = set_row_data (store, NULL, exif_categories[cat].path,
				     exif_tag_get_name (entry->tag), exif_entry_get_value (entry, b, sizeof(b)));	
		g_hash_table_insert (priv->id_path_hash,
				     GINT_TO_POINTER (entry->tag),
				     path);
	}
}

static void
exif_content_cb (ExifContent *content, gpointer data)
{
	exif_content_foreach_entry (content, exif_entry_cb, data);
}

static gboolean
clear_single_value (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gtk_tree_store_set (GTK_TREE_STORE (model), iter, MODEL_COLUMN_VALUE, " ", -1);
	return FALSE;
}

void
eog_info_view_exif_show_data (EogInfoViewExif *view, ExifData *data)
{
	EogInfoViewExifPrivate *priv;

	g_return_if_fail (EOG_IS_INFO_VIEW_EXIF (view));

	priv = view->priv;

	if (data == NULL) {
		gtk_tree_model_foreach (GTK_TREE_MODEL (priv->model), clear_single_value, NULL);
	}
	else {
		exif_data_foreach_content (data, exif_content_cb, view);
	}
}



#endif /* HAVE_EXIF */
