#include "about_dialog.h"

static const gchar license_text[] = 
  "subrec - Record audio for subtitles\n"
  "Written by Simon Berg <ksb@users.sourceforge.net>\n\n"
  "Copyright (C) 2012 Simon Berg\n\n"
  "This program is free software: you can redistribute it and/or modify "
  "it under the terms of the GNU General Public License as published by "
  "the Free Software Foundation, either version 3 of the License, or "
  "(at your option) any later version.\n"

  "This program is distributed in the hope that it will be useful, "
  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
  "GNU General Public License for more details.\n\n"
  
  "You should have received a copy of the GNU General Public License"
  "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n";

static GtkDialog *about_dialog = NULL;

static void
about_dialog_destroyed(GtkWidget *object, gpointer *user_data)
{
  about_dialog = NULL;
}

static void
about_dialog_response(GtkDialog *dialog,
		      gint response_id, gpointer *user_data)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

void
show_about_dialog(GtkWindow *parent)
{
  if (!about_dialog) {
    GtkWidget *content;
    GtkWidget *scrolled;
    GtkTextBuffer *buffer;
    GtkWidget *view;
    about_dialog =
      GTK_DIALOG(gtk_dialog_new_with_buttons ("About this program",
					      parent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      NULL));
    gtk_window_set_default_size(GTK_WINDOW(about_dialog),500,400);
    content = gtk_dialog_get_content_area(about_dialog);
    buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, license_text, -1);
    view = gtk_text_view_new_with_buffer (buffer);
    g_object_set(view,"editable", FALSE, "wrap-mode", GTK_WRAP_WORD,
		 "cursor-visible", FALSE, NULL);
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    gtk_widget_show(view);
    gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 0);
    gtk_widget_show(scrolled);
    g_signal_connect(about_dialog, "destroy",
		     G_CALLBACK(about_dialog_destroyed), NULL);
    g_signal_connect(about_dialog, "response",
		     G_CALLBACK(about_dialog_response), NULL);
  }
  gtk_widget_show(GTK_WIDGET(about_dialog));
}
  
