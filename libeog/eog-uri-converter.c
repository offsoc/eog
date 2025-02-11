#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <libgnome/gnome-macros.h>
#include <glib.h>

#include "eog-uri-converter.h"
#include "eog-pixbuf-util.h"

enum {
	PROP_0,
	PROP_CONVERT_SPACES,
	PROP_SPACE_CHARACTER,
	PROP_COUNTER_START,
	PROP_COUNTER_N_DIGITS,
	PROP_N_IMAGES
};

typedef struct {
	EogUCType  type;
	union {
		char    *string;  /* if type == EOG_UC_STRING */
		gulong  counter;  /* if type == EOG_UC_COUNTER */
	} data;
} EogUCToken;


struct _EogURIConverterPrivate {
	GnomeVFSURI     *base_uri;
	GList           *token_list;
	char            *suffix;
	GdkPixbufFormat *img_format;
	char            *counter_str;
	gboolean        requires_exif;
	
	/* options */
	gboolean convert_spaces;
	gchar    space_character;
	gulong   counter_start;
	guint    counter_n_digits;
};

static void eog_uri_converter_set_property (GObject      *object,
					    guint         property_id,
					    const GValue *value,
					    GParamSpec   *pspec);

static void eog_uri_converter_get_property (GObject    *object,
					    guint       property_id,
					    GValue     *value,
					    GParamSpec *pspec);


static void
eog_uri_converter_finalize (GObject *object)
{
	EogURIConverter *instance = EOG_URI_CONVERTER (object);
	
	g_free (instance->priv);
	instance->priv = NULL;
}

static void
free_token (gpointer data)
{
	EogUCToken *token = (EogUCToken*) data;

	if (token->type == EOG_UC_STRING && token->data.string != NULL) {
		g_free (token->data.string);
	}

	g_free (token);
}

static void
eog_uri_converter_dispose (GObject *object)
{
	EogURIConverter *instance = EOG_URI_CONVERTER (object);
	EogURIConverterPrivate *priv;

	priv = instance->priv;

	if (priv->base_uri) {
		gnome_vfs_uri_unref (priv->base_uri);
		priv->base_uri = NULL;
	}

	if (priv->token_list) {
		g_list_foreach (priv->token_list, (GFunc) free_token, NULL);
		g_list_free (priv->token_list);
		priv->token_list = NULL;
	}

	if (priv->suffix) {
		g_free (priv->suffix);
		priv->suffix = NULL;
	}

	if (priv->counter_str) {
		g_free (priv->counter_str);
		priv->counter_str = NULL;
	}
}

static void
eog_uri_converter_instance_init (EogURIConverter *obj)
{
	EogURIConverterPrivate *priv;

	priv = g_new0 (EogURIConverterPrivate, 1);

	obj->priv = priv;

	priv->convert_spaces   = FALSE;
	priv->space_character  = '_';
	priv->counter_start    = 0;
	priv->counter_n_digits = 1;
	priv->counter_str      = NULL;
	priv->requires_exif     = FALSE;
}

static void 
eog_uri_converter_class_init (EogURIConverterClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_uri_converter_finalize;
	object_class->dispose = eog_uri_converter_dispose;

        /* GObjectClass */
        object_class->set_property = eog_uri_converter_set_property;
        object_class->get_property = eog_uri_converter_get_property;

        /* Properties */
        g_object_class_install_property (
                object_class,
                PROP_CONVERT_SPACES,
                g_param_spec_boolean ("convert-spaces", NULL, NULL,
				      FALSE, G_PARAM_READWRITE));

        g_object_class_install_property (
                object_class,
                PROP_SPACE_CHARACTER,
                g_param_spec_char ("space-character", NULL, NULL,
				   ' ', '~', '_', G_PARAM_READWRITE));
	
       g_object_class_install_property (
                object_class,
                PROP_COUNTER_START,
                g_param_spec_ulong ("counter-start", NULL, NULL,
                                   0,
                                   G_MAXULONG,
                                   1,
                                   G_PARAM_READWRITE));

       g_object_class_install_property (
                object_class,
                PROP_COUNTER_N_DIGITS,
                g_param_spec_uint ("counter-n-digits", NULL, NULL,
				  1,
				  G_MAXUINT,
				  1,
				  G_PARAM_READWRITE));


       g_object_class_install_property (
                object_class,
                PROP_N_IMAGES,
                g_param_spec_uint ("n-images", NULL, NULL,
				  1,
				  G_MAXUINT,
				  1,
				  G_PARAM_WRITABLE));

}

GQuark
eog_uc_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("eog-uri-converter-error-quark");
	
	return q;
}

GNOME_CLASS_BOILERPLATE (EogURIConverter,
			 eog_uri_converter,
			 GObject,
			 G_TYPE_OBJECT);

static void 
update_counter_format_str (EogURIConverter *conv)
{
	EogURIConverterPrivate *priv;

	g_return_if_fail (EOG_IS_URI_CONVERTER (conv));
	
	priv = conv->priv;

	if (priv->counter_str != NULL) 
		g_free (priv->counter_str);

	priv->counter_str = g_strdup_printf ("%%.%ili", priv->counter_n_digits);
}


static void
eog_uri_converter_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	EogURIConverter *conv;
	EogURIConverterPrivate *priv;

        g_return_if_fail (EOG_IS_URI_CONVERTER (object));

        conv = EOG_URI_CONVERTER (object);
	priv = conv->priv;

        switch (property_id)
        {
	case PROP_CONVERT_SPACES:
		priv->convert_spaces = g_value_get_boolean (value);
		break;

	case PROP_SPACE_CHARACTER:
		priv->space_character = g_value_get_char (value);
		break;

	case PROP_COUNTER_START: 
	{
		guint new_n_digits;

		priv->counter_start = g_value_get_ulong (value);
		
		new_n_digits = ceil (log10 (priv->counter_start + pow (10, priv->counter_n_digits) - 1));
		
		if (new_n_digits != priv->counter_n_digits) {
			priv->counter_n_digits = ceil (MIN (log10 (G_MAXULONG), new_n_digits));
			update_counter_format_str (conv);
		}
		break;
	}

	case PROP_COUNTER_N_DIGITS:
		priv->counter_n_digits = ceil (MIN (log10 (G_MAXULONG), g_value_get_uint (value)));
		update_counter_format_str (conv);
		break;

	case PROP_N_IMAGES:
		priv->counter_n_digits = ceil (MIN (log10 (G_MAXULONG), 
						    log10 (priv->counter_start + g_value_get_uint (value))));
		update_counter_format_str (conv);
		break;

        default:
                g_assert_not_reached ();
        }
}

static void
eog_uri_converter_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	EogURIConverter *conv;
	EogURIConverterPrivate *priv;

        g_return_if_fail (EOG_IS_URI_CONVERTER (object));

        conv = EOG_URI_CONVERTER (object);
	priv = conv->priv;

        switch (property_id)
        {
	case PROP_CONVERT_SPACES:
		g_value_set_boolean (value, priv->convert_spaces);
		break;

	case PROP_SPACE_CHARACTER:
		g_value_set_char (value, priv->space_character);
		break;

	case PROP_COUNTER_START:
		g_value_set_ulong (value, priv->counter_start);
		break;

	case PROP_COUNTER_N_DIGITS:
		g_value_set_uint (value, priv->counter_n_digits);

        default:
                g_assert_not_reached ();
	}
}

/* parser states */
enum {
	PARSER_NONE,
	PARSER_STRING,
	PARSER_TOKEN
};

static EogUCToken*
create_token_string (const char *string, int substr_start, int substr_len) 
{
	char *start_byte;
	char *end_byte;
	int n_bytes;
	EogUCToken *token;

	if (string == NULL) return NULL;
	if (substr_len <= 0) return NULL;

	start_byte = g_utf8_offset_to_pointer (string, substr_start);
	end_byte = g_utf8_offset_to_pointer (string, substr_start + substr_len);

	/* FIXME: is this right? */
	n_bytes = end_byte - start_byte;

	token = g_new0 (EogUCToken, 1);
	token->type = EOG_UC_STRING;
	token->data.string = g_new0 (char, n_bytes);
	token->data.string = g_utf8_strncpy (token->data.string, start_byte, substr_len);
	
	return token;
}

static EogUCToken*
create_token_counter (int start_counter)
{
	EogUCToken *token;

	token = g_new0 (EogUCToken, 1);
	token->type = EOG_UC_COUNTER;
	token->data.counter = 0;

	return token;
}

static EogUCToken*
create_token_other (EogUCType type)
{
	EogUCToken *token;

	token = g_new0 (EogUCToken, 1);
	token->type = type;

	return token;
}

static GList*
eog_uri_converter_parse_string (EogURIConverter *conv, const char *string)
{
	EogURIConverterPrivate *priv;
	GList *list = NULL;
	gulong len;
	int i;
	int state = PARSER_NONE;
	int start = -1;
	int substr_len = 0;
	gunichar c;
	const char *s;
	EogUCToken *token; 

	g_return_val_if_fail (EOG_IS_URI_CONVERTER (conv), NULL);

	priv = conv->priv;
	
	if (string == NULL) return NULL;

	if (!g_utf8_validate (string, -1, NULL))
		return NULL;

	len = g_utf8_strlen (string, -1);
	s = string;

	for (i = 0; i < len; i++) {
		c = g_utf8_get_char (s);
		token = NULL;
		
		switch (state) {
		case PARSER_NONE:
			if (c == '%') {
				start = -1;
				state = PARSER_TOKEN;
			} else {
				start = i;
				substr_len = 1;
				state = PARSER_STRING;
			}
			break;

		case PARSER_STRING:
			if (c == '%') {
				if (start != -1) {
					token = create_token_string (string, start, substr_len);
				}

				state = PARSER_TOKEN;
				start = -1;
			} else {
				substr_len++;
			}
			break;

		case PARSER_TOKEN: {
			EogUCType type = EOG_UC_END;
			
			if (c == 'f') {
				type = EOG_UC_FILENAME;
			}
			else if (c == 'n') {
				type = EOG_UC_COUNTER;
				token = create_token_counter (priv->counter_start);
			}
			else if (c == 'c') {
				type = EOG_UC_COMMENT;
			}
			else if (c == 'd') {
				type = EOG_UC_DATE;
			}
			else if (c == 't') {
				type = EOG_UC_TIME;
			}
			else if (c == 'a') {
				type = EOG_UC_DAY;
			}
			else if (c == 'm') {
				type = EOG_UC_MONTH;
			}
			else if (c == 'y') {
				type = EOG_UC_YEAR;
			}
			else if (c == 'h') {
				type = EOG_UC_HOUR;
			}
			else if (c == 'i') {
				type = EOG_UC_MINUTE;
			}
			else if (c == 's') {
				type = EOG_UC_SECOND;
			}
			
			if (type != EOG_UC_END && token == NULL) {
				token = create_token_other (type); 
				priv->requires_exif = TRUE;
			}
			state = PARSER_NONE;
			break;
		}
		default:
			g_assert_not_reached ();
		}


		if (token != NULL) {
			list = g_list_append (list, token);
		}
		
		s = g_utf8_next_char (s);
	}

	if (state != PARSER_TOKEN && start >= 0) {
		/* add remaining chars as string token */
		list = g_list_append (list, create_token_string (string, start, substr_len));
	}

	return list;
}

void 
eog_uri_converter_print_list (EogURIConverter *conv)
{
	EogURIConverterPrivate *priv;
	GList *it;

	g_return_if_fail (EOG_URI_CONVERTER (conv));

	priv = conv->priv;

	for (it = priv->token_list; it != NULL; it = it->next) {
		EogUCToken *token;
		char *str;

		token = (EogUCToken*) it->data;
		
		switch (token->type) {
		case EOG_UC_STRING:
			str = g_strdup_printf ("string [%s]", token->data.string);
			break;
		case EOG_UC_FILENAME:
			str = "filename";
			break;
		case EOG_UC_COUNTER:
			str = g_strdup_printf ("counter [%li]", token->data.counter);
			break;
		case EOG_UC_COMMENT:
			str = "comment";
			break;
		case EOG_UC_DATE:
			str = "date";
			break;
		case EOG_UC_TIME:
			str = "time";
			break;
		case EOG_UC_DAY:
			str = "day";
			break;
		case EOG_UC_MONTH:
			str = "month";
			break;
		case EOG_UC_YEAR:
			str = "year";
			break;
		case EOG_UC_HOUR:
			str = "hour";
			break;
		case EOG_UC_MINUTE:
			str = "minute";
			break;
		case EOG_UC_SECOND:
			str = "second";
			break;
		default:
			str = "unknown";
			break;
		}
		
		g_print ("- %s\n", str);

		if (token->type == EOG_UC_STRING || token->type == EOG_UC_COUNTER) {
			g_free (str);
		}
	}
}


EogURIConverter*
eog_uri_converter_new (GnomeVFSURI *base_uri, GdkPixbufFormat *img_format, const char *format_str)
{
	EogURIConverter *conv;

	g_return_val_if_fail (format_str != NULL, NULL);

	conv = g_object_new (EOG_TYPE_URI_CONVERTER, NULL);
	
	if (base_uri != NULL) {
		conv->priv->base_uri  = gnome_vfs_uri_ref (base_uri);
	}
	else {
		conv->priv->base_uri = NULL;
	}
	conv->priv->img_format = img_format;
	conv->priv->token_list = eog_uri_converter_parse_string (conv, format_str);

	return conv;
}

static GnomeVFSURI*
get_uri_directory (EogURIConverter *conv, EogImage *image)
{
	GnomeVFSURI *uri = NULL;
	EogURIConverterPrivate *priv;

	g_return_val_if_fail (EOG_IS_URI_CONVERTER (conv), NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	
	priv = conv->priv;

	if (priv->base_uri != NULL) {
		uri = gnome_vfs_uri_ref (priv->base_uri);
	}
	else {
		GnomeVFSURI *img_uri;

		img_uri = eog_image_get_uri (image);
		g_assert (img_uri != NULL);

 		if (gnome_vfs_uri_has_parent (img_uri)) {
			uri = gnome_vfs_uri_get_parent (img_uri);
		}

		gnome_vfs_uri_unref (img_uri);
	}

	return uri;
}

static void
split_filename (GnomeVFSURI *uri, char **name, char **suffix)
{
	char *short_name;
	char *suffix_start;
	guint len;
		
	*name = NULL;
	*suffix = NULL;

        /* get unescaped string */
	short_name = gnome_vfs_uri_extract_short_name (uri); 

	/* FIXME: does this work for all locales? */
	suffix_start = g_utf8_strrchr (short_name, -1, '.'); 
	
	if (suffix_start == NULL) { /* no suffix found */
		*name = g_strdup (short_name);
	}
	else {
		len = (suffix_start - short_name);
		*name = g_strndup (short_name, len);

		len = MAX (0, strlen (short_name) - len - 1);
		*suffix = g_strndup (suffix_start+1, len);
	}

	g_free (short_name);
}

static GString*
append_filename (GString *str, EogImage *img)
{
	/* appends the name of the original file without 
	   filetype suffix */
	GnomeVFSURI *img_uri;
	char *name;
	char *suffix;
	GString *result;
	
	img_uri = eog_image_get_uri (img);
	split_filename (img_uri, &name, &suffix);

	result = g_string_append (str, name);

	if (name != NULL)
		g_free (name);
	if (suffix != NULL) 
		g_free (suffix);
	
	gnome_vfs_uri_unref (img_uri);

	return result;
}

static GString*
append_counter (GString *str, gulong counter,  EogURIConverter *conv)
{
	EogURIConverterPrivate *priv;

	priv = conv->priv;

	if (priv->counter_str == NULL) {
		update_counter_format_str (conv);
	}
	g_assert (priv->counter_str != NULL);
	g_string_append_printf (str, priv->counter_str, counter);

	return str;
}


static void
build_absolute_uri (EogURIConverter *conv, EogImage *image, GString *str,  /* input */
		    GnomeVFSURI **uri, GdkPixbufFormat **format)           /* output */
{ 
	GnomeVFSURI *dir_uri;
	EogURIConverterPrivate *priv;
	
	*uri = NULL;
	if (format != NULL)
		*format = NULL;

	g_return_if_fail (EOG_IS_URI_CONVERTER (conv));
	g_return_if_fail (EOG_IS_IMAGE (image));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (str != NULL);

	priv = conv->priv;

	dir_uri = get_uri_directory (conv, image);
	g_assert (dir_uri != NULL);
	
	if (priv->img_format == NULL) {
		/* use same file type/suffix */
		char *name;
		char *old_suffix;
		GnomeVFSURI *img_uri;

		img_uri = eog_image_get_uri (image);
		split_filename (img_uri, &name, &old_suffix);

		g_assert (old_suffix != NULL);

		g_string_append_unichar (str, '.');
		g_string_append (str, old_suffix);

		if (format != NULL) 
			*format = eog_pixbuf_get_format_by_suffix (old_suffix);

		gnome_vfs_uri_unref (img_uri);
	} else {
		if (priv->suffix == NULL) 
			priv->suffix = eog_pixbuf_get_common_suffix (priv->img_format);

		g_string_append_unichar (str, '.');
		g_string_append (str, priv->suffix);

		if (format != NULL) 
			*format = priv->img_format;
	}
	
	*uri = gnome_vfs_uri_append_file_name (dir_uri, str->str);
	
	gnome_vfs_uri_unref (dir_uri);
}


static GString*
replace_remove_chars (GString *str, gboolean convert_spaces, gunichar space_char)
{
	GString *result;
	guint len;
	char *s;
	int i;
	gunichar c;

	g_return_val_if_fail (str != NULL, NULL);

	if (!g_utf8_validate (str->str, -1, NULL))
	    return NULL;

	result = g_string_new (NULL);

	len = g_utf8_strlen (str->str, -1);
	s = str->str;

	for (i = 0; i < len; i++, s = g_utf8_next_char (s)) {
		c = g_utf8_get_char (s);

		if (c == '/') {
			continue;
		}
		else if (g_unichar_isspace (c) && convert_spaces) {
			result = g_string_append_unichar (result, space_char);
		}
		else {
			result = g_string_append_unichar (result, c);
		}
	}

	/* ensure maximum length of 250 characters */
	len = MIN (result->len, 250);
	result = g_string_truncate (result, len);

	return result;
}

/*
 * This function converts the uri of the EogImage object, according to the
 * EogUCToken list. The absolute uri (converted filename appended to base uri)
 * is returned in uri and the image format will be in the format pointer.
 */
gboolean
eog_uri_converter_do (EogURIConverter *conv, EogImage *image,
		      GnomeVFSURI **uri, GdkPixbufFormat **format, GError **error)
{
	EogURIConverterPrivate *priv;
	GList *it;
	GString *str;
	GString *repl_str;

	g_return_val_if_fail (EOG_IS_URI_CONVERTER (conv), FALSE);

	priv = conv->priv;

	*uri = NULL;
	if (format != NULL) 
		*format = NULL;

	str = g_string_new ("");

	for (it = priv->token_list; it != NULL; it = it->next) {
		EogUCToken *token = (EogUCToken*) it->data;

		switch (token->type) {
		case EOG_UC_STRING:
			str = g_string_append (str, token->data.string);
			break;

		case EOG_UC_FILENAME:
			str = append_filename (str, image);
			break;

		case EOG_UC_COUNTER: {
			if (token->data.counter < priv->counter_start) 
				token->data.counter = priv->counter_start;
			
			str = append_counter (str, token->data.counter++, conv);
			break;
		}
#if 0
		case EOG_UC_COMMENT:
			str = g_string_append_printf ();
			str = "comment";
			break;
		case EOG_UC_DATE:
			str = "date";
			break;
		case EOG_UC_TIME:
			str = "time";
			break;
		case EOG_UC_DAY:
			str = "day";
			break;
		case EOG_UC_MONTH:
			str = "month";
			break;
		case EOG_UC_YEAR:
			str = "year";
			break;
		case EOG_UC_HOUR:
			str = "hour";
			break;
		case EOG_UC_MINUTE:
			str = "minute";
			break;
		case EOG_UC_SECOND:
			str = "second";
			break;
#endif
		default:
		/* skip all others */
		
			break;
		}
	}

	repl_str = replace_remove_chars (str, priv->convert_spaces, priv->space_character);

	if (repl_str->len > 0) {
		build_absolute_uri (conv, image, repl_str, uri, format);
	}

	g_string_free (repl_str, TRUE);
	g_string_free (str, TRUE);
	

	return (*uri != NULL);
}


char*
eog_uri_converter_preview (const char *format_str, EogImage *img, GdkPixbufFormat *format, 
			   gulong counter, guint n_images,
			   gboolean convert_spaces, gunichar space_char)
{
	GString *str;
	GString *repl_str;
	guint n_digits;
	guint len;
	int i;
	const char *s;
	gunichar c;
	char *filename;
	gboolean token_next; 

	g_return_val_if_fail (format_str != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (img), NULL);

	if (n_images == 0) return NULL;

	n_digits = ceil (MIN (log10 (G_MAXULONG), MAX (log10 (counter), log10 (n_images))));

	str = g_string_new ("");
	
	if (!g_utf8_validate (format_str, -1, NULL))
	    return NULL;

	len = g_utf8_strlen (format_str, -1);
	s = format_str;
	token_next = FALSE;

	for (i = 0; i < len; i++, s = g_utf8_next_char (s)) {
		c = g_utf8_get_char (s);

		if (token_next) {
			if (c == 'f') {
				str = append_filename (str, img);
			}
			else if (c == 'n') {
				char *counter_format;

				counter_format = g_strdup_printf ("%%.%ili", n_digits);
				g_string_append_printf (str, counter_format, counter);

				g_free (counter_format);
			}
#if 0                   /* ignore the rest for now */
			else if (c == 'c') {
				type = EOG_UC_COMMENT;
			}
			else if (c == 'd') {
				type = EOG_UC_DATE;
			}
			else if (c == 't') {
				type = EOG_UC_TIME;
			}
			else if (c == 'a') {
				type = EOG_UC_DAY;
			}
			else if (c == 'm') {
				type = EOG_UC_MONTH;
			}
			else if (c == 'y') {
				type = EOG_UC_YEAR;
			}
			else if (c == 'h') {
				type = EOG_UC_HOUR;
			}
			else if (c == 'i') {
				type = EOG_UC_MINUTE;
			}
			else if (c == 's') {
				type = EOG_UC_SECOND;
			}
#endif
			token_next = FALSE;
		}
		else if (c == '%') {
			token_next = TRUE;
		}
		else {
			str = g_string_append_unichar (str, c);
		}
	}


	filename = NULL;
	repl_str = replace_remove_chars (str, convert_spaces, space_char);

	if (repl_str->len > 0) {
		if (format == NULL) {
			/* use same file type/suffix */
			char *name;
			char *old_suffix;
			GnomeVFSURI *img_uri;
			
			img_uri = eog_image_get_uri (img);
			split_filename (img_uri, &name, &old_suffix);

			g_assert (old_suffix != NULL);
			
			g_string_append_unichar (repl_str, '.');
			g_string_append (repl_str, old_suffix);

			gnome_vfs_uri_unref (img_uri);
		}
		else {
			char *suffix = eog_pixbuf_get_common_suffix (format);
			
			g_string_append_unichar (repl_str, '.');
			g_string_append (repl_str, suffix);

			g_free (suffix);
		}

		filename = repl_str->str;
	}

	g_string_free (repl_str, FALSE);
	g_string_free (str, TRUE);

	return filename;
}

gboolean
eog_uri_converter_requires_exif (EogURIConverter *converter)
{
	g_return_val_if_fail (EOG_IS_URI_CONVERTER (converter), FALSE);

	return converter->priv->requires_exif;
}

gboolean
eog_uri_converter_check (EogURIConverter *converter, GList *img_list, GError **error)
{
	GList *it;
	GList *uri_list = NULL;
	gboolean all_different = TRUE; 

	g_return_val_if_fail (EOG_IS_URI_CONVERTER (converter), FALSE);

	/* convert all image uris */
	for (it = img_list; it != NULL; it = it->next) {
		gboolean result;
		GnomeVFSURI *uri;
		GError *conv_error = NULL;

		result = eog_uri_converter_do (converter, EOG_IMAGE (it->data), 
					       &uri, NULL, &conv_error);

		if (result) {
			uri_list = g_list_prepend (uri_list, uri);
		}
	}

	/* check for all different uris */
	for (it = uri_list; it != NULL && all_different; it = it->next) {
		GList *p; 
		GnomeVFSURI *uri;

		uri = (GnomeVFSURI*) it->data;
		
		for (p = it->next; p != NULL && all_different; p = p->next) {
			all_different = !gnome_vfs_uri_equal (uri, (GnomeVFSURI*) p->data);
		}
	}

	if (!all_different) {
		g_set_error (error, EOG_UC_ERROR,
			     EOG_UC_ERROR_EQUAL_FILENAMES,
			     _("At least two file names are equal."));
	}

	return all_different;
}
