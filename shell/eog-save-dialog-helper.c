#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glade/glade.h>
#include <libgnome/gnome-program.h>
#include <glib/gi18n.h>
#include "eog-save-dialog-helper.h"
#include "eog-util.h"

typedef struct {
	int n_images;
	int n_processed;
	GtkWidget *header;
	GtkWidget *detail;
	GtkWidget *counter;
	GtkWidget *thumbnail;
	GtkWidget *progress;
	GtkWidget *cancel;
} SaveDialogData;


static void
update_counter_label (GtkWidget *label, int n_processed, int n_images)
{
	char *string;

	g_return_if_fail (GTK_IS_LABEL (label));

	string = g_strdup_printf ("%i/%i", n_processed, n_images);
	gtk_label_set (GTK_LABEL (label), string);

	g_free (string);
}

static void
update_progress_bar (GtkWidget *progressbar, int n_processed, int n_images)
{
	double fraction = 0.0;

	g_return_if_fail (GTK_IS_PROGRESS_BAR (progressbar));
	
	if (n_images > 0) {
		fraction = (double) n_processed / (double) n_images;
	}
	
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progressbar), fraction);
}

static void
update_thumbnail (GtkWidget *widget, EogImage *image)
{
	GdkPixbuf *pixbuf = NULL;

	g_return_if_fail (GTK_IS_IMAGE (widget));

	if (image != NULL) {
		pixbuf = eog_image_get_pixbuf_thumbnail (image);
	}

	g_object_set (GTK_IMAGE (widget), "pixbuf", pixbuf, NULL);

	if (pixbuf != NULL) 
		g_object_unref (G_OBJECT (pixbuf));
}

static gboolean
dlg_delete_cb (GtkWidget *dlg, GdkEvent *event, gpointer user_data)
{
	SaveDialogData *data;

	g_return_val_if_fail (GTK_IS_WINDOW (dlg), FALSE);

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL)
		return FALSE;

	gtk_button_clicked (GTK_BUTTON (data->cancel));

	return TRUE;
}

GtkWidget*    
eog_save_dialog_new  (GtkWindow *main, int n_images)
{
	char *filepath;
	GladeXML  *xml;
	GtkWidget *dlg;
	SaveDialogData *data;
	
	filepath = gnome_program_locate_file (NULL,
					      GNOME_FILE_DOMAIN_APP_DATADIR,
					      "eog/glade/eog.glade",
					      FALSE, NULL);
	g_assert (filepath != NULL);

	xml = glade_xml_new (filepath, "Save Image Dialog", "eog");
	g_free (filepath);
	g_assert (xml != NULL);
	
	dlg = glade_xml_get_widget (xml, "Save Image Dialog");

	data = g_new0 (SaveDialogData, 1);
	g_assert (data != NULL);

	data->n_images = n_images;
	data->n_processed = 0;
	data->header    = glade_xml_get_widget (xml, "header_label");
	data->detail    = glade_xml_get_widget (xml, "detail_label");
	data->counter   = glade_xml_get_widget (xml, "counter_label");
	data->thumbnail = glade_xml_get_widget (xml, "thumbnail");
	data->progress  = glade_xml_get_widget (xml, "progressbar");
	data->cancel    = glade_xml_get_widget (xml, "cancel_button");

	g_assert (data->header != NULL);
	g_assert (data->detail != NULL);
	g_assert (data->counter != NULL);
	g_assert (data->thumbnail != NULL);
	g_assert (data->cancel != NULL);

	gtk_label_set_text (GTK_LABEL (data->header), "");
	gtk_label_set_text (GTK_LABEL (data->detail), "");

	update_counter_label (data->counter, 0, n_images);
	update_progress_bar (data->progress, 0, n_images);
	update_thumbnail    (data->thumbnail, NULL);
	
	g_object_set_data (G_OBJECT (dlg), "data", data);
	
	gtk_window_set_transient_for (GTK_WINDOW (dlg), main);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER_ON_PARENT);

	g_signal_connect (G_OBJECT (dlg), "delete-event", (GCallback) dlg_delete_cb, NULL);

	g_object_unref (G_OBJECT (xml));

	return dlg;
}

void
eog_save_dialog_set_progress (GtkWindow *dlg, float progress)
{
	SaveDialogData *data;

	g_return_if_fail (GTK_IS_WINDOW (dlg));

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL)
		return;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (data->progress), progress);
}

void
eog_save_dialog_finished_image (GtkWindow *dlg)
{
	SaveDialogData *data;

	g_return_if_fail (GTK_IS_WINDOW (dlg));

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL)
		return;
	
	data->n_processed = MIN (data->n_processed+1, data->n_images);

	update_counter_label (data->counter, data->n_processed, data->n_images);
}

void 
eog_save_dialog_start_image (GtkWindow *dlg, EogImage *image, GnomeVFSURI *uri)
{
	SaveDialogData *data;
	char *header_str = NULL; 
	char *utf8_name = NULL;

	g_return_if_fail (GTK_IS_WINDOW (dlg));
	g_return_if_fail (EOG_IS_IMAGE (image));

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL)
		return;

	if (uri != NULL) {
		char *name;
		
		name = gnome_vfs_uri_extract_short_name (uri);
		utf8_name = eog_util_make_valid_utf8 (name);

		g_free (name);
	}
	else {
		utf8_name = g_strdup (eog_image_get_caption (image));
	}
	
	if (utf8_name != NULL) {
		header_str = g_strdup_printf (_("Saving image %s."), utf8_name);
		g_free (utf8_name);
	}

        gtk_label_set_text (GTK_LABEL (data->header), header_str);
	if (header_str != NULL) {
		g_free (header_str);
	}

	update_thumbnail     (data->thumbnail, image);
}


void
eog_save_dialog_close (GtkWindow *dlg, gboolean successful)
{
	SaveDialogData *data;

	g_return_if_fail (GTK_IS_WINDOW (dlg));

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data != NULL && successful) {
		update_progress_bar (data->progress, data->n_images, data->n_images);
		update_counter_label (data->counter, data->n_images, data->n_images);

		g_timeout_add (1000, (GSourceFunc) gtk_widget_destroy, dlg);
	}
	else {
		gtk_widget_destroy (GTK_WIDGET (dlg));
	}

	if (data != NULL) 
		g_free (data);
}

void 
eog_save_dialog_cancel (GtkWindow *dlg)
{
	SaveDialogData *data;

	g_return_if_fail (GTK_IS_WINDOW (dlg));

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL) return;

	gtk_widget_set_sensitive (GTK_WIDGET (data->cancel), FALSE);
	gtk_label_set_text (GTK_LABEL (data->header), _("Cancel saving ..."));
	gtk_label_set_text (GTK_LABEL (data->detail), "");
	update_thumbnail (GTK_WIDGET (data->thumbnail), NULL);
}

GtkWidget*
eog_save_dialog_get_button (GtkWindow *dlg)
{
	SaveDialogData *data;

	g_return_val_if_fail (GTK_IS_WINDOW (dlg), NULL);

	data = (SaveDialogData*) g_object_get_data (G_OBJECT (dlg), "data");
	if (data == NULL)
		return NULL;

	return data->cancel;
}
