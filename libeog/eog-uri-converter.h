#ifndef _EOG_URI_CONVERTER_H_
#define _EOG_URI_CONVERTER_H_

#include <glib-object.h>
#include <glib/gi18n.h>
#include "eog-image.h"

G_BEGIN_DECLS

#define EOG_TYPE_URI_CONVERTER            (eog_uri_converter_get_type ())
#define EOG_URI_CONVERTER(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_URI_CONVERTER, EogURIConverter))
#define EOG_URI_CONVERTER_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_URI_CONVERTER, EogURIConverterClass))
#define EOG_IS_URI_CONVERTER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_URI_CONVERTER))
#define EOG_IS_URI_CONVERTER_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_URI_CONVERTER))
#define EOG_URI_CONVERTER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_URI_CONVERTER, EogURIConverterClass))

typedef struct _EogURIConverter EogURIConverter;
typedef struct _EogURIConverterClass EogURIConverterClass;
typedef struct _EogURIConverterPrivate EogURIConverterPrivate;

typedef enum {
	EOG_UC_STRING,
	EOG_UC_FILENAME,
	EOG_UC_COUNTER,
	EOG_UC_COMMENT,
	EOG_UC_DATE,
	EOG_UC_TIME,
	EOG_UC_DAY,
	EOG_UC_MONTH,
	EOG_UC_YEAR,
	EOG_UC_HOUR,
	EOG_UC_MINUTE,
	EOG_UC_SECOND,
	EOG_UC_END
} EogUCType;

typedef struct {
	char     *description;
	char     *rep;
	gboolean req_exif;
} EogUCInfo;

static const EogUCInfo uc_info[] = {
	{ "String",      "",   FALSE }, /* only used for internal purpose */
	{ N_("Filename"),"%f", FALSE },
	{ N_("Counter"), "%n", FALSE },
	{ N_("Comment"), "%c", TRUE },
	{ N_("Date"),    "%d", TRUE },
	{ N_("Time"),    "%t", TRUE },
	{ N_("Day"),     "%a", TRUE },
	{ N_("Month"),   "%m", TRUE },
	{ N_("Year"),    "%y", TRUE },
	{ N_("Hour"),    "%h", TRUE },
	{ N_("Minute"),  "%i", TRUE },
	{ N_("Second"),  "%s", TRUE },
	{ NULL, NULL, FALSE }
};

typedef enum {
	EOG_UC_ERROR_INVALID_UNICODE,
	EOG_UC_ERROR_INVALID_CHARACTER,
	EOG_UC_ERROR_EQUAL_FILENAMES,
	EOG_UC_ERROR_UNKNOWN
} EogUCError;

#define EOG_UC_ERROR eog_uc_error_quark ()


struct _EogURIConverter {
	GObject parent;

	EogURIConverterPrivate *priv;
};

struct _EogURIConverterClass {
	GObjectClass parent_klass;
};

GType               eog_uri_converter_get_type                       (void) G_GNUC_CONST;
GQuark              eog_uc_error_quark (void);

EogURIConverter*    eog_uri_converter_new        (GnomeVFSURI *base_uri, GdkPixbufFormat *img_format, const char *format_string);

gboolean            eog_uri_converter_check      (EogURIConverter *converter, GList *img_list, GError **error);

gboolean            eog_uri_converter_requires_exif (EogURIConverter *converter);

gboolean            eog_uri_converter_do         (EogURIConverter *converter, EogImage *image,
						  GnomeVFSURI **uri, GdkPixbufFormat **format, GError **error);

char*               eog_uri_converter_preview    (const char *format_str, EogImage *img, GdkPixbufFormat *format, 
						  gulong counter, guint n_images,
						  gboolean convert_spaces, gunichar space_char);


/* for debugging purpose only */
void                eog_uri_converter_print_list (EogURIConverter *conv);

G_END_DECLS

#endif /* _EOG_URI_CONVERTER_H_ */
