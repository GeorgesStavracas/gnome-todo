/* gtd-storage.c
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

#include "gtd-task-list.h"
#include "gtd-storage.h"

#include <glib/gi18n.h>

typedef struct
{
  GIcon                 *icon;
  gchar                 *id;
  gchar                 *name;
  gchar                 *provider;
  gchar                 *provider_name;

  gint                   enabled : 1;
  gint                   is_default : 1;
} GtdStoragePrivate;

struct _GtdStorage
{
  GObject            parent;

  /*<private>*/
  GtdStoragePrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdStorage, gtd_storage, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ENABLED,
  PROP_ICON,
  PROP_ID,
  PROP_IS_DEFAULT,
  PROP_NAME,
  PROP_PROVIDER,
  PROP_PROVIDER_NAME,
  LAST_PROP
};

static void
gtd_storage_finalize (GObject *object)
{
  GtdStorage *self = (GtdStorage *)object;
  GtdStoragePrivate *priv = gtd_storage_get_instance_private (self);

  g_clear_object (&priv->icon);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->provider, g_free);
  g_clear_pointer (&priv->provider_name, g_free);

  G_OBJECT_CLASS (gtd_storage_parent_class)->finalize (object);
}

static void
gtd_storage_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtdStorage *self = GTD_STORAGE (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->priv->enabled);
      break;

    case PROP_ICON:
      g_value_set_object (value, gtd_storage_get_icon (self));
      break;

    case PROP_ID:
      g_value_set_string (value, self->priv->id);
      break;

    case PROP_IS_DEFAULT:
      g_value_set_boolean (value, self->priv->is_default);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->priv->name);
      break;

    case PROP_PROVIDER:
      g_value_set_string (value, self->priv->provider);
      break;

    case PROP_PROVIDER_NAME:
      g_value_set_string (value, self->priv->provider_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtdStorage *self = GTD_STORAGE (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gtd_storage_set_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_ID:
      if (self->priv->id)
        g_free (self->priv->id);

      self->priv->id = g_strdup (g_value_get_string (value));

      g_object_notify (object, "id");
      break;

    case PROP_IS_DEFAULT:
      gtd_storage_set_is_default (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      gtd_storage_set_name (self, g_value_get_string (value));
      break;

    case PROP_PROVIDER:
      gtd_storage_set_provider (self, g_value_get_string (value));
      break;

    case PROP_PROVIDER_NAME:
      gtd_storage_set_provider_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_storage_class_init (GtdStorageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtd_storage_finalize;
  object_class->get_property = gtd_storage_get_property;
  object_class->set_property = gtd_storage_set_property;

  /**
   * GtdStorage::enabled:
   *
   * Whether the storage is available to be used.
   */
  g_object_class_install_property (
        object_class,
        PROP_ENABLED,
        g_param_spec_boolean ("enabled",
                              _("Whether the storage is enabled"),
                              _("Whether the storage is available to be used."),
                              FALSE,
                              G_PARAM_READWRITE));

  /**
   * GtdStorage::icon:
   *
   * The icon representing this storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_ICON,
        g_param_spec_object ("icon",
                             _("Icon of the storage"),
                             _("The icon representing the storage location."),
                             G_TYPE_ICON,
                             G_PARAM_READABLE));

  /**
   * GtdStorage::id:
   *
   * The unique identifier of this storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_ID,
        g_param_spec_string ("id",
                             _("Identifier of the storage"),
                             _("The unique identifier of the storage location."),
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtdStorage::is-default:
   *
   * Whether the storage is the default to be used.
   */
  g_object_class_install_property (
        object_class,
        PROP_IS_DEFAULT,
        g_param_spec_boolean ("is-default",
                              _("Whether the storage is the default"),
                              _("Whether the storage is the default storage location to be used."),
                              FALSE,
                              G_PARAM_READWRITE));

  /**
   * GtdStorage::name:
   *
   * The user-visible name of this storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_NAME,
        g_param_spec_string ("name",
                             _("Name of the storage"),
                             _("The user-visible name of the storage location."),
                             NULL,
                             G_PARAM_READWRITE));

  /**
   * GtdStorage::provider:
   *
   * The provider of this storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_PROVIDER,
        g_param_spec_string ("provider",
                             _("Provider type of the storage"),
                             _("The provider type of the storage location."),
                             NULL,
                             G_PARAM_READWRITE));

  /**
   * GtdStorage::provider-name:
   *
   * The name of the provider of this storage.
   */
  g_object_class_install_property (
        object_class,
        PROP_PROVIDER_NAME,
        g_param_spec_string ("provider-name",
                             _("Provider name of the storage"),
                             _("The name of the provider of the storage location."),
                             NULL,
                             G_PARAM_READWRITE));
}

static void
gtd_storage_init (GtdStorage *self)
{
  self->priv = gtd_storage_get_instance_private (self);
}

/**
 * gtd_storage_new:
 * @id: a %NULL-terminated string
 * @name: a %NULL-terminated string
 *
 * Creates a new #GtdStorage from the given @id.
 *
 * Returns: (transfer full): a new #GtdStorage
 */
GtdStorage*
gtd_storage_new (const gchar *id,
                 const gchar *provider,
                 const gchar *provider_name,
                 const gchar *name)
{
  return g_object_new (GTD_TYPE_STORAGE,
                       "id", id,
                       "name", name,
                       "provider", provider,
                       "provider-name", provider_name,
                       NULL);
}

/**
 * gtd_storage_get_enabled:
 * @storage: a #GtdStorage
 *
 * Whether @storage is available for creating new #GtdTaskList.
 *
 * Returns: %TRUE if the storage is available for use, %FALSE otherwise.
 */
gboolean
gtd_storage_get_enabled (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), FALSE);

  return storage->priv->enabled;
}

/**
 * gtd_storage_set_enabled:
 * @storage: a #GtdStorage
 * @enabled: %TRUE to make it available for use, %FALSE otherwise.
 *
 * Sets the #GtdStorage::enabled property to @enabled.
 *
 * Returns:
 */
void
gtd_storage_set_enabled (GtdStorage *storage,
                         gboolean    enabled)
{
  g_return_if_fail (GTD_IS_STORAGE (storage));

  if (storage->priv->enabled != enabled)
    {
      storage->priv->enabled = enabled;

      g_object_notify (G_OBJECT (storage), "enabled");
    }
}

/**
 * gtd_storage_get_icon:
 * @storage: a #GtdStorage
 *
 * The #GIcon that represents this storage location. A provider
 * type must be given, or this function returns %NULL.
 *
 * Returns: (transfer none): the #GIcon that represents this
 * storage location, or %NULL.
 */
GIcon*
gtd_storage_get_icon (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  return storage->priv->icon;
}

/**
 * gtd_storage_get_id:
 * @storage: a #GtdStorage
 *
 * Retrieves the unique identifier of @storage.
 *
 * Returns: (transfer none): the unique identifier of @storage.
 */
const gchar*
gtd_storage_get_id (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  return storage->priv->id;
}

/**
 * gtd_storage_get_is_default:
 * @storage: a #GtdStorage
 *
 * Whether @storage is the default #GtdStorage or not.
 *
 * Returns: %TRUE is @storage is the default storage, %FALSE otherwise.
 */
gboolean
gtd_storage_get_is_default (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), FALSE);

  return storage->priv->is_default;
}

/**
 * gtd_storage_set_is_default:
 * @storage: a #GtdStorage
 * @is_default: %TRUE to make @storage the default one, %FALSE otherwise
 *
 * Sets the #GtdStorage::is-default property.
 *
 * Returns:
 */
void
gtd_storage_set_is_default (GtdStorage *storage,
                            gboolean    is_default)
{
  GtdStoragePrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = storage->priv;

  if (priv->is_default != is_default)
    {
      priv->is_default = is_default;

      g_object_notify (G_OBJECT (storage), "is-default");
    }
}

/**
 * gtd_storage_get_name:
 * @storage: a #GtdStorage
 *
 * Retrieves the user-visible name of @storage
 *
 * Returns: (transfer none): the user-visible name of @storage
 */
const gchar*
gtd_storage_get_name (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  return storage->priv->name;
}

/**
 * gtd_storage_set_name:
 * @storage: a #GtdStorage
 *
 * Sets the user visible name of @storage.
 *
 * Returns:
 */
void
gtd_storage_set_name (GtdStorage  *storage,
                      const gchar *name)
{
  GtdStoragePrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = storage->priv;

  if (g_strcmp0 (priv->name, name) != 0)
    {
      g_clear_pointer (&priv->name, g_free);

      priv->name = g_strdup (name);

      g_object_notify (G_OBJECT (storage), "name");
    }
}

/**
 * gtd_storage_get_provider:
 * @storage: a #GtdStorage
 *
 * Retrieves the storage provider of @storage.
 *
 * Returns: (transfer none): retrieves the storage provider of @storage.
 */
const gchar*
gtd_storage_get_provider (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  return storage->priv->provider;
}

/**
 * gtd_storage_set_provider:
 * @storage: a #GtdStorage
 * @provider: the storage provider for @storage.
 *
 * Sets the provider of @storage. This directly affects the
 * @GtdStorage::icon property.
 *
 * Returns:
 */
void
gtd_storage_set_provider (GtdStorage  *storage,
                          const gchar *provider)
{
  GtdStoragePrivate *priv;

  g_return_if_fail (GTD_IS_STORAGE (storage));

  priv = storage->priv;

  if (g_strcmp0 (priv->provider, provider) != 0)
    {
      g_clear_pointer (&priv->provider, g_free);
      g_clear_object (&priv->icon);

      priv->provider = g_strdup (provider);

      if (g_strcmp0 (provider, "local") == 0)
        {
          priv->icon = G_ICON (g_themed_icon_new ("computer-symbolic"));
        }
      else
        {
          gchar *icon_name;

          icon_name = g_strdup_printf ("goa-account-%s", provider);

          priv->icon = G_ICON (g_themed_icon_new (icon_name));

          g_free (icon_name);
        }

      g_object_notify (G_OBJECT (storage), "provider");
    }
}

/**
 * gtd_storage_get_provider_name:
 * @storage: a #GtdStorage
 *
 * Retrieves the provider name of @storage.
 *
 * Returns: (transfer none): the provider name of @storage.
 */
const gchar*
gtd_storage_get_provider_name (GtdStorage *storage)
{
  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  return storage->priv->provider_name;
}

/**
 * gtd_storage_set_provider_name:
 * @storage: a #GtdStorage
 * @provider_name: the name of the provider.
 *
 * Sets the #GtdStorage::provider-name property.
 *
 * Returns:
 */
void
gtd_storage_set_provider_name (GtdStorage  *storage,
                               const gchar *provider_name)
{
  g_return_if_fail (GTD_IS_STORAGE (storage));

  if (g_strcmp0 (storage->priv->provider_name, provider_name) != 0)
    {
      g_clear_pointer (&storage->priv->provider_name, g_free);

      storage->priv->provider_name = g_strdup (provider_name);

      g_object_notify (G_OBJECT (storage), "provider-name");
    }
}

/**
 * gtd_storage_compare:
 * @a: a #GtdStorage
 * @b: a #GtdStorage
 *
 * Compare @a and @b
 *
 * Returns: -1 if @a comes before @b, 0 if they're equal, 1 otherwise.
 */
gint
gtd_storage_compare (GtdStorage *a,
                     GtdStorage *b)
{
  gint retval;

  g_return_val_if_fail (GTD_IS_STORAGE (a), 0);
  g_return_val_if_fail (GTD_IS_STORAGE (b), 0);

  /* Compare by providers first */
  retval = g_strcmp0 (gtd_storage_get_provider (b), gtd_storage_get_provider (a));

  if (retval != 0)
    return -1 * retval;

  /* Then, by identifiers */
  retval = g_strcmp0 (gtd_storage_get_id (b), gtd_storage_get_id (a));

  return -1 * retval;
}

GtdTaskList*
gtd_storage_create_task_list (GtdStorage  *storage,
                              const gchar *name)
{
  GtdStoragePrivate *priv;
  ESourceExtension *extension;
  GtdTaskList *task_list;
  ESource *source;
  GError *error;

  g_return_val_if_fail (GTD_IS_STORAGE (storage), NULL);

  priv = storage->priv;
  error = NULL;
  source = e_source_new (NULL,
                         NULL,
                         &error);

  if (error)
    {
      g_warning ("%s: %s: %s",
                 G_STRFUNC,
                 _("Error creating new task list"),
                 error->message);

      g_clear_error (&error);
      return NULL;
    }

  /* Some properties */
  e_source_set_display_name (source, name);

  /*
   * The only special case is "local" provider, which is treated
   * differently here.
   */
  if (g_strcmp0 (priv->provider_name, "local") == 0)
    {
      extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);

      e_source_set_parent (source, "local-stub");
      e_source_backend_set_backend_name (E_SOURCE_BACKEND (extension), "local");
    }
  else
    {
      /* TODO: create source for GOA account */
      extension = e_source_get_extension (source, E_SOURCE_EXTENSION_TASK_LIST);
    }

  /* Create task list */
  task_list = gtd_task_list_new (source, priv->name);

  return task_list;
}
