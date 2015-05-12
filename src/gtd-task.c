/* gtd-task.c
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

#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>
#include <libecal/libecal.h>
#include <libical/icaltime.h>
#include <libical/icaltimezone.h>

typedef struct
{
  gchar           *description;
  GtdTaskList     *list;
  ECalComponent   *component;
} GtdTaskPrivate;

struct _GtdTask
{
  GtdObject parent;

  /*<private>*/
  GtdTaskPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtdTask, gtd_task, GTD_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_COMPLETE,
  PROP_COMPONENT,
  PROP_DESCRIPTION,
  PROP_DUE_DATE,
  PROP_LIST,
  PROP_PRIORITY,
  PROP_TITLE,
  LAST_PROP
};

GDateTime*
gtd_task__convert_icaltime (const icaltimetype *date,
                            const gchar        *tz)
{
  GDateTime *dt;
  gboolean is_date;

  g_return_val_if_fail (date, NULL);

  is_date = date->is_date ? TRUE : FALSE;

  if (tz)
    {
      GTimeZone *timezone = g_time_zone_new (tz);

      dt = g_date_time_new (timezone,
                            date->year,
                            date->month,
                            date->day,
                            is_date ? date->hour : 0,
                            is_date ? date->minute : 0,
                            is_date ? date->second : 0.0);

      g_time_zone_unref (timezone);
    }
  else
    {
      dt = g_date_time_new_local (date->year,
                                  date->month,
                                  date->day,
                                  is_date ? date->hour : 0,
                                  is_date ? date->minute : 0,
                                  is_date ? date->second : 0.0);
    }

  return dt;
}

static void
gtd_task_finalize (GObject *object)
{
  GtdTask *self = (GtdTask*) object;

  if (self->priv->description)
    g_free (self->priv->description);

  if (self->priv->component)
    g_object_unref (self->priv->component);

  G_OBJECT_CLASS (gtd_task_parent_class)->finalize (object);
}

static const gchar*
gtd_task__get_uid (GtdObject *object)
{
  GtdTaskPrivate *priv = GTD_TASK (object)->priv;
  const gchar *uid;

  g_return_val_if_fail (GTD_IS_TASK (object), NULL);

  if (priv->component)
    e_cal_component_get_uid (priv->component, &uid);
  else
    uid = NULL;

  return uid;
}

static void
gtd_task__set_uid (GtdObject   *object,
                   const gchar *uid)
{
  GtdTaskPrivate *priv = GTD_TASK (object)->priv;
  const gchar *current_uid;

  g_return_if_fail (GTD_IS_TASK (object));

  if (!priv->component)
    return;

  e_cal_component_get_uid (priv->component, &current_uid);

  if (g_strcmp0 (current_uid, uid) != 0)
    {
      e_cal_component_set_uid (priv->component, uid);

      g_object_notify (G_OBJECT (object), "uid");
    }
}

static void
gtd_task_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtdTask *self = GTD_TASK (object);

  switch (prop_id)
    {
    case PROP_COMPLETE:
      g_value_set_boolean (value, gtd_task_get_complete (self));
      break;

    case PROP_COMPONENT:
      g_value_set_object (value, self->priv->component);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, self->priv->description);
      break;

    case PROP_DUE_DATE:
      g_value_set_boxed (value, gtd_task_get_due_date (self));
      break;

    case PROP_LIST:
      g_value_set_object (value, self->priv->list);
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, gtd_task_get_priority (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtd_task_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtdTask *self = GTD_TASK (object);

  switch (prop_id)
    {
    case PROP_COMPLETE:
      gtd_task_set_complete (self, g_value_get_boolean (value));
      break;

    case PROP_COMPONENT:
      self->priv->component = g_value_get_object (value);

      if (!self->priv->component)
        {
          self->priv->component = e_cal_component_new ();
          e_cal_component_set_new_vtype (self->priv->component, E_CAL_COMPONENT_TODO);
        }
      else
        {
          g_object_ref (self->priv->component);
        }

      break;

    case PROP_DESCRIPTION:
      gtd_task_set_description (self, g_value_get_string (value));
      break;

    case PROP_DUE_DATE:
      gtd_task_set_due_date (self, g_value_get_boxed (value));
      break;

    case PROP_LIST:
      gtd_task_set_list (self, g_value_get_object (value));
      break;

    case PROP_PRIORITY:
      gtd_task_set_priority (self, g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtd_task_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_task_class_init (GtdTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtdObjectClass *obj_class = GTD_OBJECT_CLASS (klass);

  object_class->finalize = gtd_task_finalize;
  object_class->get_property = gtd_task_get_property;
  object_class->set_property = gtd_task_set_property;

  obj_class->get_uid = gtd_task__get_uid;
  obj_class->set_uid = gtd_task__set_uid;

  /**
   * GtdTask::complete:
   *
   * @TRUE if the task is marked as complete or @FALSE otherwise. Usually
   * represented by a checkbox at user interfaces.
   */
  g_object_class_install_property (
        object_class,
        PROP_COMPLETE,
        g_param_spec_boolean ("complete",
                              _("Whether the task is completed or not"),
                              _("Whether the task is marked as completed by the user"),
                              FALSE,
                              G_PARAM_READWRITE));

  /**
   * GtdTask::component:
   *
   * The #ECalComponent of the task.
   */
  g_object_class_install_property (
        object_class,
        PROP_COMPONENT,
        g_param_spec_object ("component",
                              _("Component of the task"),
                              _("The #ECalComponent this task handles."),
                              E_TYPE_CAL_COMPONENT,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtdTask::description:
   *
   * Description of the task.
   */
  g_object_class_install_property (
        object_class,
        PROP_DESCRIPTION,
        g_param_spec_string ("description",
                             _("Description of the task"),
                             _("Optional string describing the task"),
                             NULL,
                             G_PARAM_READWRITE));

  /**
   * GtdTask::due-date:
   *
   * The @GDateTime that represents the time in which the task should
   * be completed before.
   */
  g_object_class_install_property (
        object_class,
        PROP_DUE_DATE,
        g_param_spec_boxed ("due-date",
                            _("End date of the task"),
                            _("The day the task is supposed to be completed"),
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE));

  /**
   * GtdTask::list:
   *
   * The @GtdTaskList that contains this task.
   */
  g_object_class_install_property (
        object_class,
        PROP_LIST,
        g_param_spec_object ("list",
                             _("List of the task"),
                             _("The list that owns this task"),
                             GTD_TYPE_TASK_LIST,
                             G_PARAM_READWRITE));

  /**
   * GtdTask::priority:
   *
   * Priority of the task, 0 if not set.
   */
  g_object_class_install_property (
        object_class,
        PROP_PRIORITY,
        g_param_spec_int ("priority",
                          _("Priority of the task"),
                          _("The priority of the task. 0 means no priority set, and tasks will be sorted alfabetically."),
                          0,
                          G_MAXINT,
                          0,
                          G_PARAM_READWRITE));

  /**
   * GtdTask::title:
   *
   * The title of the task, usually the task name.
   */
  g_object_class_install_property (
        object_class,
        PROP_TITLE,
        g_param_spec_string ("title",
                             _("Title of the task"),
                             _("The title of the task"),
                             NULL,
                             G_PARAM_READWRITE));
}

static void
gtd_task_init (GtdTask *self)
{
  self->priv = gtd_task_get_instance_private (self);
}

GtdTask *
gtd_task_new (ECalComponent *component)
{
  const gchar *uid;

  if (component)
    e_cal_component_get_uid (component, &uid);
  else
    uid = NULL;

  return g_object_new (GTD_TYPE_TASK,
                       "component", component,
                       NULL);
}

/**
 * gtd_task_get_complete:
 * @task: a #GtdTask
 *
 * Retrieves whether the task is complete or not.
 *
 * Returns: %TRUE if the task is complete, %FALSE otherwise
 */
gboolean
gtd_task_get_complete (GtdTask *task)
{
  icaltimetype *dt;
  gboolean completed;

  g_return_val_if_fail (GTD_IS_TASK (task), FALSE);

  e_cal_component_get_completed (task->priv->component, &dt);
  completed = (dt != NULL);

  if (dt)
    e_cal_component_free_icaltimetype (dt);

  return completed;
}

ECalComponent*
gtd_task_get_component (GtdTask *task)
{
  g_return_val_if_fail (GTD_IS_TASK (task), NULL);

  return task->priv->component;
}

/**
 * gtd_task_set_complete:
 * @task: a #GtdTask
 * @complete: the new value
 *
 * Updates the complete state of @task.
 *
 * Returns:
 */
void
gtd_task_set_complete (GtdTask  *task,
                       gboolean  complete)
{
  g_assert (GTD_IS_TASK (task));

  if (gtd_task_get_complete (task) != complete)
    {
      icaltimetype *dt;
      icalproperty_status status;
      gint percent;

      if (complete)
        {
          GDateTime *now = g_date_time_new_now_local ();
          icaltimezone *tz;

          percent = 100;
          status = ICAL_STATUS_COMPLETED;

          dt = g_new0 (icaltimetype, 1);
          dt->year = g_date_time_get_year (now);
          dt->month = g_date_time_get_month (now);
          dt->day = g_date_time_get_day_of_month (now);
          dt->hour = g_date_time_get_hour (now);
          dt->minute = g_date_time_get_minute (now);
          dt->second = g_date_time_get_seconds (now);
          dt->is_date = 0;
          dt->is_utc = 1;

          /* convert timezone */
          icaltimezone_convert_time (dt,
                                     tz,
                                     icaltimezone_get_utc_timezone ());

        }
      else
        {
          dt = NULL;
          percent = 0;
          status = ICAL_STATUS_NEEDSACTION;
        }

      e_cal_component_set_percent_as_int (task->priv->component, percent);
      e_cal_component_set_status (task->priv->component, status);
      e_cal_component_set_completed (task->priv->component, dt);

      if (dt)
        e_cal_component_free_icaltimetype (dt);

      g_object_notify (G_OBJECT (task), "complete");
    }
}

/**
 * gtd_task_get_description:
 * @task: a #GtdTask
 *
 * Retrieves the description of the task.
 *
 * Returns: (transfer none): the description of @task
 */
const gchar*
gtd_task_get_description (GtdTask *task)
{
  GSList *text_list;
  GSList *l;
  gchar *desc = NULL;

  g_return_val_if_fail (GTD_IS_TASK (task), NULL);

  /* concatenates the multiple descriptions a task may have */
  e_cal_component_get_description_list (task->priv->component, &text_list);
  for (l = text_list; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        {
          ECalComponentText *text;
          gchar *carrier;
          text = l->data;

          if (desc != NULL)
            {
              carrier = g_strconcat (desc,
                                     "\n",
                                     text->value,
                                     NULL);
              g_free (desc);
              desc = carrier;
            }
          else
            {
              desc = g_strdup (text->value);
            }
        }
    }

  if (g_strcmp0 (task->priv->description, desc) != 0)
    {
      if (task->priv->description)
        g_free (task->priv->description);

      task->priv->description = g_strdup (desc);
    }

  g_free (desc);
  e_cal_component_free_text_list (text_list);

  return task->priv->description ? task->priv->description : "";
}

/**
 * gtd_task_set_description:
 * @task: a #GtdTask
 * @description: the new description, or %NULL
 *
 * Updates the description of @task. The string is not stripped off of
 * spaces to preserve user data.
 *
 * Returns:
 */
void
gtd_task_set_description (GtdTask     *task,
                          const gchar *description)
{
  g_assert (GTD_IS_TASK (task));
  g_assert (g_utf8_validate (description, -1, NULL));

  if (g_strcmp0 (task->priv->description, description) != 0)
    {
      GSList note;
      ECalComponentText text;

      if (task->priv->description)
        g_free (task->priv->description);

      task->priv->description = g_strdup (description);
      text.value = task->priv->description;
      text.altrep = NULL;
      note.data = &text;
      note.next = NULL;

      e_cal_component_set_description_list (task->priv->component, &note);

      g_object_notify (G_OBJECT (task), "description");
    }
}

/**
 * gtd_task_get_due_date:
 * @task: a #GtdTask
 *
 * Returns the #GDateTime that represents the task's due date.
 * The value is referenced for thread safety. Returns %NULL if
 * no date is set.
 *
 * Returns: (transfer full): the internal #GDateTime referenced
 * for thread safety, or %NULL. Unreference it after use.
 */
GDateTime*
gtd_task_get_due_date (GtdTask *task)
{
  ECalComponentDateTime comp_dt;

  g_return_val_if_fail (GTD_IS_TASK (task), NULL);

  e_cal_component_get_dtend (task->priv->component, &comp_dt);

  if (comp_dt.value)
    return gtd_task__convert_icaltime (comp_dt.value, comp_dt.tzid);
  else
    return NULL;
}

/**
 * gtd_task_set_due_date:
 * @task: a #GtdTask
 * @dt: a #GDateTime
 *
 * Updates the internal @GtdTask::due-date property.
 *
 * Returns:
 */
void
gtd_task_set_due_date (GtdTask   *task,
                       GDateTime *dt)
{
  GDateTime *current_dt;

  g_assert (GTD_IS_TASK (task));

  current_dt = gtd_task_get_due_date (task);

  if (dt != current_dt)
    {
      ECalComponentDateTime comp_dt;
      icaltimetype *idt;

      comp_dt.value = NULL;
      comp_dt.tzid = NULL;

      if (!current_dt ||
          (current_dt &&
           dt &&
           g_date_time_compare (current_dt, dt) != 0))
        {
          idt = g_new0 (icaltimetype, 1);

          g_date_time_ref (dt);

          /* Copy the given dt */
          idt->year = g_date_time_get_year (dt);
          idt->month = g_date_time_get_month (dt);
          idt->day = g_date_time_get_day_of_month (dt);
          idt->hour = g_date_time_get_hour (dt);
          idt->minute = g_date_time_get_minute (dt);
          idt->second = g_date_time_get_seconds (dt);
          idt->is_date = (idt->hour == 0 &&
                          idt->minute == 0 &&
                          idt->second == 0);

          comp_dt.tzid = g_strdup (g_date_time_get_timezone_abbreviation (dt));

          g_date_time_unref (dt);

          g_object_notify (G_OBJECT (task), "due-date");
        }
      else if (!dt)
        {
          idt = NULL;
          comp_dt.tzid = NULL;

          g_object_notify (G_OBJECT (task), "due-date");
        }

      comp_dt.value = idt;

      e_cal_component_set_dtend (task->priv->component, &comp_dt);

      e_cal_component_free_datetime (&comp_dt);
    }

  if (current_dt)
    g_date_time_unref (current_dt);
}

/**
 * gtd_task_get_list:
 *
 * Returns a weak reference to the #GtdTaskList that
 * owns the given @task.
 *
 * Returns: (transfer none): a weak reference to the
 * #GtdTaskList that owns @task. Do not free after
 * usage.
 */
GtdTaskList*
gtd_task_get_list (GtdTask *task)
{
  g_return_val_if_fail (GTD_IS_TASK (task), NULL);

  return task->priv->list;
}

/**
 * gtd_task_set_list:
 *
 * Sets the parent #GtdTaskList of @task.
 *
 * Returns:
 */
void
gtd_task_set_list (GtdTask     *task,
                   GtdTaskList *list)
{
  g_assert (GTD_IS_TASK (task));
  g_assert (GTD_IS_TASK_LIST (list));

  if (task->priv->list != list)
    {
      task->priv->list = list;
      g_object_notify (G_OBJECT (task), "list");
    }
}

/**
 * gtd_task_get_priority:
 * @task: a #GtdTask
 *
 * Returns the priority of @task inside the parent #GtdTaskList,
 * or -1 if not set.
 *
 * Returns: the priority of the task, or 0
 */
gint
gtd_task_get_priority (GtdTask *task)
{
  gint *priority = NULL;
  gint p;

  g_assert (GTD_IS_TASK (task));

  e_cal_component_get_priority (task->priv->component, &priority);

  if (!priority)
    return -1;

  p = *priority;

  g_free (priority);

  return p;
}

/**
 * gtd_task_set_priority:
 * @task: a #GtdTask
 * @priority: the priority of @task, or -1
 *
 * Sets the @task priority inside the parent #GtdTaskList. It
 * is up to the interface to handle two or more #GtdTask with
 * the same priority value.
 *
 * Returns:
 */
void
gtd_task_set_priority (GtdTask *task,
                       gint     priority)
{
  gint current;

  g_assert (GTD_IS_TASK (task));
  g_assert (priority >= -1);

  current = gtd_task_get_priority (task);

  if (priority != current)
    {
      e_cal_component_set_priority (task->priv->component, &priority);
      g_object_notify (G_OBJECT (task), "priority");
    }
}

/**
 * gtd_task_get_title:
 * @task: a #GtdTask
 *
 * Retrieves the title of the task, or %NULL.
 *
 * Returns: (transfer none): the title of @task, or %NULL
 */
const gchar*
gtd_task_get_title (GtdTask *task)
{
  GtdTaskPrivate *priv = task->priv;
  ECalComponentText summary;

  g_return_val_if_fail (GTD_IS_TASK (task), NULL);
  g_return_val_if_fail (E_IS_CAL_COMPONENT (priv->component), NULL);

  e_cal_component_get_summary (task->priv->component, &summary);

  return summary.value;
}

/**
 * gtd_task_set_title:
 * @task: a #GtdTask
 * @description: the new title, or %NULL
 *
 * Updates the title of @task. The string is stripped off of
 * leading spaces.
 *
 * Returns:
 */
void
gtd_task_set_title (GtdTask     *task,
                    const gchar *title)
{
  ECalComponentText summary;

  g_return_if_fail (GTD_IS_TASK (task));
  g_return_if_fail (g_utf8_validate (title, -1, NULL));

  e_cal_component_get_summary (task->priv->component, &summary);

  if (g_strcmp0 (summary.value, title) != 0)
    {
      ECalComponentText new_summary;

      new_summary.value = title;
      new_summary.altrep = NULL;

      e_cal_component_set_summary (task->priv->component, &new_summary);

      g_object_notify (G_OBJECT (task), "title");
    }
}

/**
 * gtd_task_abort:
 * @task: a #GtdTask
 *
 * Cancels any editing made on @task after the latest
 * call of @gtd_task_save.
 *
 * Returns:
 */
void
gtd_task_abort (GtdTask *task)
{
  g_return_if_fail (GTD_IS_TASK (task));
  g_return_if_fail (E_IS_CAL_COMPONENT (task->priv->component));

  e_cal_component_abort_sequence (task->priv->component);
}

/**
 * gtd_task_save:
 * @task: a #GtdTask
 *
 * Save any changes made on @task.
 *
 * Returns:
 */
void
gtd_task_save (GtdTask *task)
{
  g_return_if_fail (GTD_IS_TASK (task));
  g_return_if_fail (E_IS_CAL_COMPONENT (task->priv->component));

  e_cal_component_commit_sequence (task->priv->component);
}

gint
gtd_task_compare (GtdTask *t1,
                  GtdTask *t2)
{
  GDateTime *dt1;
  GDateTime *dt2;
  gboolean completed1;
  gboolean completed2;
  gint p1;
  gint p2;
  gint retval;

  if (!t1 && !t2)
    return  0;
  if (!t1)
    return  1;
  if (!t2)
    return -1;

  /*
   * First, compare by ::complete.
   */
  completed1 = gtd_task_get_complete (t1);
  completed2 = gtd_task_get_complete (t2);
  retval = completed1 - completed2;

  if (retval != 0)
    return retval;

  /*
   * Second, compare by ::priority
   */
  p1 = gtd_task_get_priority (t1);
  p2 = gtd_task_get_priority (t2);

  retval = p2 - p1;

  if (retval != 0)
    return retval;

  /*
   * Third, compare by ::due-date.
   */
  dt1 = gtd_task_get_due_date (t1);
  dt2 = gtd_task_get_due_date (t2);

  if (!dt1 && !dt2)
    retval =  0;
  else if (!dt1)
    retval =  1;
  else if (!dt2)
    retval = -1;
  else
    retval = -1 * g_date_time_compare (t1, t2);

  if (dt1)
    g_date_time_unref (dt1);
  if (dt2)
    g_date_time_unref (dt2);

  if (retval != 0)
    return retval;

  /*
   * If they're equal up to now, compare by title.
   */
  return g_strcmp0 (gtd_task_get_title (t1), gtd_task_get_title (t2));
}
