/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2010 Nokia Corporation.
 * Contact: Xun Chen <xun.chen@nokia.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "proplist.h"
#include "event.h"
#include "context.h"

#define GROUP_GENERAL    "general"
#define GROUP_VIBRATOR   "vibra"
#define GROUP_DEFINITION "definition"
#define GROUP_EVENT      "event"

#define ARRAY_SIZE(x) (sizeof (x) / sizeof (x[0]))

enum
{
    KEY_ENTRY_TYPE_STRING,
    KEY_ENTRY_TYPE_INT,
    KEY_ENTRY_TYPE_BOOL
};

typedef struct _KeyEntry
{
    guint       type;
    const char *key;
    gint        def_int;
    const char *def_str;
} KeyEntry;

typedef struct _SettingsData
{
    Context    *context;
    GHashTable *groups;
    GHashTable *events;
} SettingsData;

static KeyEntry event_entries[] = {
    /* general */
    { KEY_ENTRY_TYPE_INT   , "max_timeout"          , 0    , NULL },
    { KEY_ENTRY_TYPE_BOOL  , "allow_custom"         , FALSE, NULL },
    { KEY_ENTRY_TYPE_INT   , "dummy"                , 0    , NULL },

    /* sound */
    { KEY_ENTRY_TYPE_BOOL  , "audio_enabled"        , FALSE, NULL },
    { KEY_ENTRY_TYPE_BOOL  , "audio_repeat"         , FALSE, NULL },
    { KEY_ENTRY_TYPE_INT   , "audio_max_repeats"    , 0    , NULL },
    { KEY_ENTRY_TYPE_STRING, "sound"                , 0    , NULL },
    { KEY_ENTRY_TYPE_BOOL  , "silent_enabled"       , FALSE, NULL },
    { KEY_ENTRY_TYPE_STRING, "volume"               , 0    , NULL },
    { KEY_ENTRY_TYPE_STRING, "event_id"             , 0    , NULL },

    /* tonegen */
    { KEY_ENTRY_TYPE_BOOL  , "audio_tonegen_enabled", FALSE, NULL },
    { KEY_ENTRY_TYPE_INT   , "audio_tonegen_pattern", -1   , NULL },

    /* vibration */
    { KEY_ENTRY_TYPE_BOOL  , "vibration_enabled"    , FALSE, NULL },
    { KEY_ENTRY_TYPE_BOOL  , "lookup_pattern"       , FALSE, NULL },
    { KEY_ENTRY_TYPE_STRING, "vibration"            , FALSE, NULL },

    /* led */
    { KEY_ENTRY_TYPE_BOOL  , "led_enabled"          , FALSE, NULL },
    { KEY_ENTRY_TYPE_STRING, "led_pattern"          , 0    , NULL },

    /* backlight */
    { KEY_ENTRY_TYPE_BOOL  , "backlight_enabled"    , FALSE, NULL }
};



static gchar*
_strip_group_type (const char *group)
{
    gchar *ptr = NULL;

    ptr = (gchar*) group;
    while (*ptr != '\0' && *ptr != ' ')
        ptr++;

    if (*ptr == ' ')
        ptr++;

    if (*ptr == '\0')
        return NULL;

    return g_strdup (ptr);
}

static gchar*
_parse_group_name (const char *group)
{
    gchar **split  = NULL;
    gchar  *name   = NULL;
    gchar  *result = NULL;

    name = _strip_group_type (group);
    if (name == NULL)
        return NULL;

    split = g_strsplit (name, "@", 2);
    if (split[0] == NULL) {
        g_strfreev (split);
        g_free (name);
        return NULL;
    }

    result = g_strdup (split[0]);
    g_strfreev (split);
    g_free (name);
    return result;
}

static gchar*
_parse_group_parent (const char *group)
{
    gchar **split  = NULL;
    gchar  *parent = NULL;
    gchar  *name   = NULL;

    if (group == NULL)
        return NULL;

    name = _strip_group_type (group);
    if (name == NULL)
        return NULL;

    split = g_strsplit (name, "@", 2);
    if (split[0] == NULL || split[1] == NULL) {
        g_strfreev (split);
        g_free (name);
        return NULL;
    }

    if (split[1] != NULL)
        parent = g_strdup (split[1]);

    g_strfreev (split);
    g_free (name);
    return parent;
}

static void
_parse_required_plugins (SettingsData *data, GKeyFile *k)
{
    Context  *context = data->context;
    gchar    *value   = NULL;
    gchar   **arr     = NULL;
    gchar   **name    = NULL;

    value = g_key_file_get_string (k, GROUP_GENERAL, "plugins", NULL);
    if (!value)
        return;

    arr = g_strsplit (value, " ", -1);
    for (name = arr; *name; ++name)
        context->required_plugins = g_list_append (context->required_plugins, g_strdup (*name));
}

static void
_parse_general (SettingsData *data, GKeyFile *k)
{
    Context *context = data->context;
    gchar *value = NULL;
    gchar **split  = NULL;
    gchar **item = NULL;
    guint i = 0;

    _parse_required_plugins (data, k);

    context->patterns_path      = g_key_file_get_string (k, GROUP_GENERAL, "vibration_search_path", NULL);
    context->sound_path      = g_key_file_get_string (k, GROUP_GENERAL, "sound_search_path", NULL);
    context->audio_buffer_time  = g_key_file_get_integer (k, GROUP_GENERAL, "buffer_time", NULL);
    context->audio_latency_time = g_key_file_get_integer (k, GROUP_GENERAL, "latency_time", NULL);

    value = g_key_file_get_string (k, GROUP_GENERAL, "system_volume", NULL);

    split = g_strsplit (value, ";", -1);
    if (split[0] == NULL) {
        g_strfreev (split);
        g_free (value);
        return;
    }

    item = split;

    for (i=0;i<3;i++) {
        if (*item == NULL) {
            g_strfreev (split);
            g_free (value);
            return;
        }
        context->system_volume[i] = atoi (*item);
        item++;
    }

    g_strfreev (split);
    g_free (value);
}

static gchar*
_check_path (const char *basename, const char *search_path)
{
    gchar *path;

    if (g_file_test (basename, G_FILE_TEST_EXISTS))
        return g_strdup (basename);

    path = g_build_filename (search_path, basename, NULL);

    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

    g_free (path);

    return NULL;
}

static void
_parse_definitions (SettingsData *data, GKeyFile *k)
{
    Context *context = data->context;

    gchar **group_list = NULL;
    gchar **group      = NULL;
    gchar  *name       = NULL;

    Definition *def = NULL;

    /* For each group that begins with GROUP_DEFINITION, get the values for long and
       short events. */

    group_list = g_key_file_get_groups (k, NULL);
    for (group = group_list; *group != NULL; ++group) {
        if (!g_str_has_prefix (*group, GROUP_DEFINITION))
            continue;

        name = _parse_group_name (*group);
        if (name == NULL)
            continue;

        def = definition_new ();
        def->long_event    = g_key_file_get_string (k, *group, "long", NULL);
        def->short_event   = g_key_file_get_string (k, *group, "short", NULL);
        def->meeting_event = g_key_file_get_string (k, *group, "meeting", NULL);

        N_DEBUG ("<new definition> %s (long=%s, short=%s, meeting=%s)", name, def->long_event, def->short_event, def->meeting_event);
        g_hash_table_replace (context->definitions, g_strdup (name), def);
    }

    g_strfreev (group_list);
}

static gboolean
_event_is_done (GList *done_list, const char *name)
{
    GList *iter = NULL;

    for (iter = g_list_first (done_list); iter; iter = g_list_next (iter)) {
        if (iter->data && name && g_str_equal ((const char*) iter->data, name))
            return TRUE;
    }

    return FALSE;
}

static void
_add_property_int (NProplist  *proplist,
                   GKeyFile   *k,
                   const char *group,
                   const char *key,
                   gint        def_value,
                   gboolean    set_default)
{
    GError   *error  = NULL;
    gint      result = 0;

    result = g_key_file_get_integer (k, group, key, &error);
    if (error != NULL) {
        if (error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
            N_WARNING ("Invalid value for property %s, expected integer. Using default value %d", key, def_value);
        g_error_free (error);
        result = def_value;

        if (!set_default)
            return;
    }

    n_proplist_set_int (proplist, key, result);
}

static void
_add_property_bool (NProplist  *proplist,
                    GKeyFile   *k,
                    const char *group,
                    const char *key,
                    gboolean    def_value,
                    gboolean    set_default)
{
    GError   *error  = NULL;
    gboolean  result = FALSE;

    result = g_key_file_get_boolean (k, group, key, &error);
    if (error != NULL) {
        if (error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
            N_WARNING ("Invalid value for property %s, expected boolean. Using default value %s", key, def_value ? "TRUE" : "FALSE");
        g_error_free (error);
        result = def_value;

        if (!set_default)
            return;
    }

    n_proplist_set_bool (proplist, key, result);
}

static void
_add_property_string (NProplist  *proplist,
                      GKeyFile   *k,
                      const char *group,
                      const char *key,
                      const char *def_value,
                      gboolean    set_default)
{
    GError   *error  = NULL;
    gchar    *result = NULL;

    result = g_key_file_get_string (k, group, key, &error);
    if (error != NULL) {
        if (error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
            N_WARNING ("Invalid value for property %s, expected string. Using default value %s", key, def_value);
        g_error_free (error);

        if (!set_default)
            return;

        result = g_strdup (def_value);
    }

    n_proplist_set_string (proplist, key, result);
    g_free (result);
}

static gchar*
_strip_prefix (const gchar *str, const gchar *prefix)
{
    if (!g_str_has_prefix (str, prefix))
        return NULL;

    size_t prefix_length = strlen (prefix);
    return g_strdup (str + prefix_length);
}

static gboolean
_parse_profile_key (const char *key, gchar **out_profile, gchar **out_key)
{
    gchar **split = NULL;
    gboolean ret = FALSE;

    if (key == NULL)
        return FALSE;

    split = g_strsplit (key, "@", 2);
    if (split[0] == NULL) {
        ret = FALSE;
        goto done;
    }

    *out_key = g_strdup (split[0]);
    *out_profile = g_strdup (split[1]);
    ret = TRUE;

done:
    g_strfreev (split);
    return ret;
}

static SoundPath*
_parse_sound_path (Context *context, const gchar *str)
{
    SoundPath *sound_path = NULL;
    gchar *stripped = NULL;

    if (str == NULL)
        return NULL;

    if (g_str_has_prefix (str, "profile:")) {
        stripped = _strip_prefix (str, "profile:");

        sound_path       = sound_path_new ();
        sound_path->type = SOUND_PATH_TYPE_PROFILE;

        if (!_parse_profile_key (stripped, &sound_path->profile, &sound_path->key)) {
            g_free (stripped);
            sound_path_free (sound_path);
            return NULL;
        }

        g_free (stripped);
    }
    else if (g_str_has_prefix (str, "filename:")) {
        stripped = _strip_prefix (str, "filename:");

        sound_path           = sound_path_new ();
        sound_path->type     = SOUND_PATH_TYPE_FILENAME;
        sound_path->filename = _check_path (stripped, context->sound_path);
        g_free (stripped);
        if (sound_path->filename == NULL) {
            sound_path_free (sound_path);
            return NULL;
        }
    }

    return context_add_sound_path (context, sound_path);
}

static GList*
_create_sound_paths (Context *context, const gchar *str)
{
    GList      *result = NULL;
    SoundPath  *sound_path = NULL;
    gchar     **sounds = NULL, **s = NULL;

    if (str == NULL)
        return NULL;

    sounds = g_strsplit (str, ";", -1);
    if (sounds[0] == NULL) {
        g_strfreev (sounds);
        return NULL;
    }

    for (s = sounds; *s; ++s) {
        if ((sound_path = _parse_sound_path (context, *s)) != NULL)
            result = g_list_append (result, sound_path);
    }

    g_strfreev (sounds);
    return result;
}

static Volume*
_create_volume (Context *context, const gchar *str)
{
    Volume *volume = NULL;
    gchar *stripped = NULL;
    gchar **split = NULL;
    gchar **item = NULL;
    gint i = 0;

    if (str == NULL)
        return NULL;

    if (g_str_has_prefix (str, "profile:")) {
        stripped = _strip_prefix (str, "profile:");

        volume       = volume_new ();
        volume->type = VOLUME_TYPE_PROFILE;

        if (!_parse_profile_key (stripped, &volume->profile, &volume->key)) {
            g_free (stripped);
            volume_free (volume);
            return NULL;
        }

        g_free (stripped);
    }
    else if (g_str_has_prefix (str, "fixed:")) {
        stripped = _strip_prefix (str, "fixed:");

        volume        = volume_new ();
        volume->type  = VOLUME_TYPE_FIXED;
        volume->level = atoi (stripped);

        g_free (stripped);
    }
    else if (g_str_has_prefix (str, "linear:")) {
        stripped = _strip_prefix (str, "linear:");

        volume             = volume_new ();
        volume->type       = VOLUME_TYPE_LINEAR;
        volume->level = 100;

        split = g_strsplit (stripped, ";", -1);
        if (split[0] == NULL) {
            g_strfreev (split);
            g_free (stripped);
            volume_free (volume);
            return NULL;
        }

        item = split;

        for (i=0;i<3;i++) {
            if (*item == NULL) {
                g_strfreev (split);
                g_free (stripped);
                volume_free (volume);
                return NULL;
            }
            volume->linear[i] = atoi (*item);        
            item++;
        }

        g_strfreev (split);
        g_free (stripped);
    }

    return context_add_volume (context, volume);
}

static VibrationPattern*
_parse_pattern (Context *context, const gchar *str)
{
    VibrationPattern *pattern = NULL;
    gchar *stripped = NULL;

    if (str == NULL)
        return NULL;

    if (g_str_has_prefix (str, "profile:")) {
        stripped = _strip_prefix (str, "profile:");

        pattern = vibration_pattern_new ();
        pattern->type     = VIBRATION_PATTERN_TYPE_PROFILE;

        if (!_parse_profile_key (stripped, &pattern->profile, &pattern->key)) {
            g_free (stripped);
            vibration_pattern_free (pattern);
            return NULL;
        }

        g_free (stripped);
    }
    else if (g_str_has_prefix (str, "filename:")) {
        stripped = _strip_prefix (str, "filename:");

        pattern = vibration_pattern_new ();
        pattern->type     = VIBRATION_PATTERN_TYPE_FILENAME;
        pattern->filename = _check_path (stripped, context->patterns_path);
        g_free (stripped);
        if (pattern->filename == NULL) {
            vibration_pattern_free (pattern);
            return NULL;
        }
    }
    else if (g_str_has_prefix (str, "internal:")) {
        stripped = _strip_prefix (str, "internal:");

        pattern = vibration_pattern_new ();
        pattern->type    = VIBRATION_PATTERN_TYPE_INTERNAL;
        pattern->pattern = atoi (stripped);

        g_free (stripped);
    }

    return context_add_pattern (context, pattern);
}

static GList*
_create_patterns (Context *context, const gchar *str)
{
    GList             *result = NULL;
    VibrationPattern  *pattern = NULL;
    gchar            **patterns = NULL, **i = NULL;

    if (str == NULL)
        return NULL;

    patterns = g_strsplit (str, ";", -1);
    if (patterns[0] == NULL) {
        g_strfreev (patterns);
        return NULL;
    }

    for (i = patterns; *i; ++i) {
        if ((pattern = _parse_pattern (context, *i)) != NULL)
            result = g_list_append (result, pattern);
    }

    g_strfreev (patterns);
    return result;
}

static void
_parse_single_event (SettingsData *data, GKeyFile *k, GList **events_done, const char *name)
{
    const gchar *group      = NULL;
    gchar       *parent     = NULL;
    KeyEntry    *entry      = NULL;
    gboolean     is_base    = FALSE;
    NProplist   *proplist   = NULL;
    NProplist   *copy       = NULL;
    guint        i          = 0;

    if (name == NULL)
        return;

    if (_event_is_done (*events_done, name))
        return;

    if ((group = g_hash_table_lookup (data->groups, name)) == NULL)
        return;

    if ((parent = _parse_group_parent (group)) != NULL)
        _parse_single_event (data, k, events_done, parent);

    proplist = n_proplist_new ();

    for (i = 0; i < ARRAY_SIZE(event_entries); ++i) {
        entry = &event_entries[i];

        is_base = (parent == NULL) ? TRUE : FALSE;
        switch (entry->type) {
            case KEY_ENTRY_TYPE_STRING:
                _add_property_string (proplist, k, group, entry->key, entry->def_str, is_base);
                break;
            case KEY_ENTRY_TYPE_INT:
                _add_property_int (proplist, k, group, entry->key, entry->def_int, is_base);
                break;
            case KEY_ENTRY_TYPE_BOOL:
                _add_property_bool (proplist, k, group, entry->key, entry->def_int, is_base);
                break;
            default:
                break;
        }
    }

    /* if a parent was defined, merge */
    if (parent != NULL) {
        copy = n_proplist_copy (g_hash_table_lookup (data->events, parent));
        n_proplist_merge (copy, proplist);
        n_proplist_free (proplist);
        proplist = copy;
    }

    g_hash_table_insert (data->events, g_strdup (name), proplist);
    *events_done = g_list_append (*events_done, g_strdup (name));
    g_free (parent);
}

static void
finalize_event (SettingsData *data, const char *name, NProplist *proplist)
{
    Context *context = data->context;
    Event   *event   = event_new ();

    if (!name)
        return;

    event->audio_enabled          = n_proplist_get_bool (proplist, "audio_enabled");
    event->vibration_enabled      = n_proplist_get_bool (proplist, "vibration_enabled");
    event->leds_enabled           = n_proplist_get_bool (proplist, "led_enabled");
    event->backlight_enabled      = n_proplist_get_bool (proplist, "backlight_enabled");

    event->allow_custom           = n_proplist_get_bool (proplist, "allow_custom");
    event->max_timeout            = n_proplist_get_int  (proplist, "max_timeout");
    event->lookup_pattern         = n_proplist_get_bool (proplist, "lookup_pattern");
    event->silent_enabled         = n_proplist_get_bool (proplist, "silent_enabled");
    event->event_id               = g_strdup (n_proplist_get_string (proplist, "event_id"));

    event->tone_generator_enabled = n_proplist_get_bool (proplist, "audio_tonegen_enabled");
    event->tone_generator_pattern = n_proplist_get_int (proplist, "audio_tonegen_pattern");

    event->repeat                 = n_proplist_get_bool (proplist, "audio_repeat");
    event->num_repeats            = n_proplist_get_int (proplist, "audio_max_repeats");
    event->led_pattern            = g_strdup (n_proplist_get_string (proplist, "led_pattern"));

    event->sounds                 = _create_sound_paths (context, n_proplist_get_string (proplist, "sound"));
    event->volume                 = _create_volume      (context, n_proplist_get_string (proplist, "volume"));
    event->patterns               = _create_patterns    (context, n_proplist_get_string (proplist, "vibration"));

    g_hash_table_replace (context->events, g_strdup (name), event);
}

static void
_parse_events (SettingsData *data, GKeyFile *k)
{
    gchar      **group_list = NULL;
    gchar      **group      = NULL;
    gchar       *name       = NULL;
    NProplist   *evtdata    = NULL;

    data->events = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_destroy);
    data->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    /* Get the available events and map name to group */

    group_list = g_key_file_get_groups (k, NULL);
    for (group = group_list; *group != NULL; ++group) {
        if (!g_str_has_prefix (*group, GROUP_EVENT))
            continue;

        if ((name = _parse_group_name (*group)) != NULL)
            g_hash_table_insert (data->groups, name, g_strdup (*group));
    }

    g_strfreev (group_list);

    /* For each entry in the map of events ... */

    GHashTableIter iter;
    gchar *key, *value;
    GList *events_done = NULL;

    g_hash_table_iter_init (&iter, data->groups);
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value))
        _parse_single_event (data, k, &events_done, key);

    g_list_foreach (events_done, (GFunc) g_free, NULL);
    g_list_free (events_done);

    g_hash_table_iter_init (&iter, data->events);
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &evtdata))
        finalize_event (data, key, evtdata);

    g_hash_table_destroy (data->groups);
    g_hash_table_destroy (data->events);
}

gboolean
load_settings (Context *context)
{
    static const char *conf_files[] = { "/etc/ngf/ngf.ini", "./ngf.ini", NULL };

    SettingsData  *data     = NULL;
    GKeyFile      *key_file = NULL;
    const char   **filename = NULL;
    gboolean       success  = FALSE;

    if ((key_file = g_key_file_new ()) == NULL)
        return FALSE;

    for (filename = conf_files; *filename != NULL; ++filename) {
        if (g_key_file_load_from_file (key_file, *filename, G_KEY_FILE_NONE, NULL)) {
            success = TRUE;
            break;
        }
    }

    if (!success)
        return FALSE;

    data = g_new0 (SettingsData, 1);
    data->context = context;

    _parse_general     (data, key_file);
    _parse_definitions (data, key_file);
    _parse_events      (data, key_file);

    g_free (data);

    return TRUE;
}
