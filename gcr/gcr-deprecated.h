/*
 * gnome-keyring
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#if !defined (__GCR_H_INSIDE__) && !defined (GCR_COMPILATION)
#error "Only <gcr/gcr.h> can be included directly."
#endif


#ifndef GCR_DEPRECATED_H_
#define GCR_DEPRECATED_H_

#include <glib.h>

#include <gck/gck.h>

#include "gcr-importer.h"
#include "gcr-parser.h"
#include "gcr-simple-collection.h"
#include "gcr-viewer.h"

#ifndef GCR_DISABLE_DEPRECATED

#include "gcr-certificate-basics-widget.h"
#include "gcr-certificate-details-widget.h"

void              gcr_renderer_render                         (GcrRenderer *self,
                                                               GcrViewer *viewer);

GcrParser*        gcr_importer_get_parser                     (GcrImporter *self);

void              gcr_importer_set_parser                     (GcrImporter *self,
                                                               GcrParser *parser);

#define           GCR_ERROR                                   (gcr_error_get_domain ())

GQuark            gcr_error_get_domain                        (void) G_GNUC_CONST;

gboolean          gcr_simple_collection_contains              (GcrSimpleCollection *self,
                                                               GObject *object);

#endif /* GCR_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* GCRTYPES_H_ */