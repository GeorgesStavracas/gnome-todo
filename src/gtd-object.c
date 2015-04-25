/* gtd-object.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gtd-object.h"

#include <glib/gi18n.h>

typedef struct
{
  gboolean       loaded;
  gchar         *uid;
} GtdObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtdObject, gtd_object, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_LOADED,
  PROP_UID,
  LAST_PROP
};

static void
gtd_object_finalize (GObject *object)
{
  GtdObject *self = GTD_OBJECT (object);
  GtdObjectPrivate *priv = gtd_object_get_instance_private (self);

  if (priv->uid)
    g_free (priv->uid);

  G_OBJECT_CLASS (gtd_object_parent_class)->finalize (object);
}

static void
gtd_object_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtdObject *self = GTD_OBJECT (object);
  GtdObjectPrivate *priv = gtd_object_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_LOADED:
      g_value_set_boolean (value, priv->loaded);
      break;

    case PROP_UID:
      g_value_set_string (value, priv->uid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_object_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtdObject *self = GTD_OBJECT (object);

  switch (prop_id)
    {
    case PROP_LOADED:
      gtd_object_set_loaded (self, g_value_get_boolean (value));
      break;

    case PROP_UID:
      gtd_object_set_uid (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_object_class_init (GtdObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtd_object_finalize;
  object_class->get_property = gtd_object_get_property;
  object_class->set_property = gtd_object_set_property;

  /**
   * GtdObject::uid:
   *
   * The unique identified of the object, set by the backend.
   */
  g_object_class_install_property (
        object_class,
        PROP_UID,
        g_param_spec_string ("uid",
                             _("Unique identifier of the object"),
                             _("The unique identifier of the object, defined by the backend"),
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
gtd_object_init (GtdObject *self)
{
}

/**
 * gtd_object_new:
 * @uid: unique identifier of the object
 *
 * Creates a new #GtdObject object.
 *
 * Returns: (transfer full): a new #GtdObject
 */
GtdObject*
gtd_object_new (const gchar *uid)
{
  return g_object_new (GTD_TYPE_OBJECT, "uid", uid, NULL);
}

/**
 * gtd_object_get_uid:
 * @object: a #GtdObject
 *
 * Retrieves the internal unique identifier of @object.
 *
 * Returns: (transfer none): the unique identifier of @object. Do
 * not free after usage.
 */
const gchar*
gtd_object_get_uid (GtdObject *object)
{
  GtdObjectPrivate *priv;

  g_return_val_if_fail (GTD_IS_OBJECT (object), NULL);

  priv = gtd_object_get_instance_private (object);

  return priv->uid;
}

/**
 * gtd_object_set_uid:
 *
 * Sets the unique identifier of @object to @uid. Only
 * a #GtdBackend should do it.
 *
 * Returns:
 */
void
gtd_object_set_uid (GtdObject   *object,
                    const gchar *uid)
{
  GtdObjectPrivate *priv;

  g_assert (GTD_IS_OBJECT (object));

  priv = gtd_object_get_instance_private (object);

  if (g_strcmp0 (priv->uid, uid) != 0)
    {
      if (priv->uid)
        g_free (priv->uid);

      priv->uid = g_strdup (uid);

      g_object_notify (G_OBJECT (object), "uid");
    }
}

/**
 * gtd_object_get_loaded:
 * @object: a #GtdObject
 *
 * Whether @object is loaded by the #GtdBackend.
 *
 * Returns: %TRUE if @object is completely loaded by the backend,
 * %FALSE otherwise.
 */
gboolean
gtd_object_get_loaded (GtdObject *object)
{
  GtdObjectPrivate *priv;

  g_return_val_if_fail (GTD_IS_OBJECT (object), NULL);

  priv = gtd_object_get_instance_private (object);

  return priv->loaded;
}

/**
 * gtd_object_set_loaded:
 *
 * Sets the GtdObject::loaded property to @loaded. Only the
 * #GtdBackend should do it.
 *
 * Returns:
 */
void
gtd_object_set_loaded (GtdObject *object,
                       gboolean   loaded)
{
  GtdObjectPrivate *priv;

  g_assert (GTD_IS_OBJECT (object));

  priv = gtd_object_get_instance_private (object);

  if (priv->loaded != loaded)
    {
      priv->loaded = loaded;

      g_object_notify (G_OBJECT (object), "loaded");
    }
}
