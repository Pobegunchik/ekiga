
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2008 Damien Sandras
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */


/*
 *                        simple-chat-page.cpp  -  description
 *                         --------------------------------
 *   begin                : written in july 2008 by Julien Puydt
 *   copyright            : (C) 2008 by Julien Puydt
 *   description          : Declaration of a page displaying a SimpleChat
 *
 */

#include "simple-chat-page.h"
#include "chat-area.h"

static GObjectClass *parent_class = NULL;

static void
simple_chat_page_dispose (GObject *obj)
{

  parent_class->dispose (obj);
}

static void
simple_chat_page_finalize (GObject *obj)
{

  parent_class->finalize (obj);
}

static void
simple_chat_page_init (GTypeInstance* instance,
		       G_GNUC_UNUSED gpointer g_class)
{
  SimpleChatPage* self = NULL;

  self = (SimpleChatPage*)instance;
}

static void
simple_chat_page_class_init (gpointer g_class,
			     G_GNUC_UNUSED gpointer class_data)
{
  GObjectClass* gobject_class = NULL;

  parent_class = (GObjectClass*)g_type_class_peek_parent (g_class);

  gobject_class = (GObjectClass*)g_class;
  gobject_class->dispose = simple_chat_page_dispose;
  gobject_class->finalize = simple_chat_page_finalize;
}

GType
simple_chat_page_get_type ()
{
  static GType result = 0;
  if (!result) {
    static const GTypeInfo my_info = {
      sizeof(SimpleChatPageClass),
      NULL,
      NULL,
      simple_chat_page_class_init,
      NULL,
      NULL,
      sizeof (SimpleChatPage),
      1,
      simple_chat_page_init,
      NULL
    };

    result = g_type_register_static (GTK_TYPE_VBOX,
				      "SimpleChatPage",
				     &my_info, (GTypeFlags)0);
  }
  return result;
}

/* implementation of the public api */

GtkWidget*
simple_chat_page_new (Ekiga::SimpleChat& chat)
{
  SimpleChatPage* result = NULL;
  GtkWidget* area = NULL;

  result = (SimpleChatPage*)g_object_new (TYPE_SIMPLE_CHAT_PAGE, NULL);

  area = chat_area_new (chat);
  gtk_box_pack_start (GTK_BOX (result), area,
		      TRUE,TRUE, 2);
  gtk_widget_show (area);

  return GTK_WIDGET (result);
}