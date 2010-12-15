/*
 * Copyright (C) 2010 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 * 
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option) any
 * later version. See http://www.gnu.org/copyleft/lgpl.html the full text of the
 * license.
 */

#include "user.h"

enum {
    PROP_0,
    PROP_GREETER,
    PROP_NAME,
    PROP_REAL_NAME,
    PROP_DISPLAY_NAME,
    PROP_IMAGE,
    PROP_LANGUAGE,
    PROP_LAYOUT,
    PROP_SESSION,
    PROP_LOGGED_IN
};

struct _LdmUserPrivate
{
    LdmGreeter *greeter;

    gchar *name;
    gchar *real_name;
    gchar *image;
    gboolean logged_in;

    gboolean have_defaults;
    gchar *language;
    gchar *layout;
    gchar *session;
};

G_DEFINE_TYPE (LdmUser, ldm_user, G_TYPE_OBJECT);

/**
 * ldm_user_new:
 * 
 * Create a new user.
 * @greeter: The greeter the user is connected to
 * @name: The username
 * @real_name: The real name of the user
 * @image: The image URI
 * @logged_in: TRUE if this user is currently logged in
 * 
 * Return value: the new #LdmUser
 **/
LdmUser *
ldm_user_new (LdmGreeter *greeter, const gchar *name, const gchar *real_name, const gchar *image, gboolean logged_in)
{
    return g_object_new (LDM_TYPE_USER, "greeter", greeter, "name", name, "real-name", real_name, "image", image, "logged-in", logged_in, NULL);
}

/**
 * ldm_user_get_name:
 * @user: A #LdmUser
 * 
 * Get the name of a user.
 * 
 * Return value: The name of the given user
 **/
const gchar *
ldm_user_get_name (LdmUser *user)
{
    return user->priv->name;
}

/**
 * ldm_user_get_real_name:
 * @user: A #LdmUser
 * 
 * Get the real name of a user.
 *
 * Return value: The real name of the given user (may be blank)
 **/
const gchar *
ldm_user_get_real_name (LdmUser *user)
{
    return user->priv->real_name;
}

/**
 * ldm_user_get_display_name:
 * @user: A #LdmUser
 * 
 * Get the display name of a user.
 * 
 * Return value: The display name of the given user
 **/
const gchar *
ldm_user_get_display_name (LdmUser *user)
{
    if (user->priv->real_name[0] != '\0')
        return user->priv->real_name;
    else
        return user->priv->name;
}

/**
 * ldm_user_get_image:
 * @user: A #LdmUser
 * 
 * Get the image URI for a user.
 * 
 * Return value: The image URI for the given user or NULL if no URI
 **/
const gchar *
ldm_user_get_image (LdmUser *user)
{
    return user->priv->image;
}

static void
get_defaults (LdmUser *user)
{
    if (user->priv->have_defaults)
        return;

    if (ldm_greeter_get_user_defaults (user->priv->greeter, user->priv->name, &user->priv->language, &user->priv->layout, &user->priv->session))
        user->priv->have_defaults = TRUE;
}

/**
 * ldm_user_get_language
 * @user: A #LdmUser
 * 
 * Get the language for a user.
 * 
 * Return value: The language for the given user or NULL if using system defaults.
 **/
const gchar *
ldm_user_get_language (LdmUser *user)
{
    get_defaults (user);
    return user->priv->language;
}

/**
 * ldm_user_get_layout
 * @user: A #LdmUser
 * 
 * Get the keyboard layout for a user.
 * 
 * Return value: The keyboard layoyt for the given user or NULL if using system defaults.
 **/
const gchar *
ldm_user_get_layout (LdmUser *user)
{
    get_defaults (user);
    return user->priv->layout;
}

/**
 * ldm_user_get_session
 * @user: A #LdmUser
 * 
 * Get the session for a user.
 * 
 * Return value: The session for the given user or NULL if using system defaults.
 **/
const gchar *
ldm_user_get_session (LdmUser *user)
{
    get_defaults (user);
    return user->priv->session; 
}

/**
 * ldm_user_get_logged_in:
 * @user: A #LdmUser
 * 
 * Check if a user is logged in.
 * 
 * Return value: TRUE if the user is currently logged in.
 **/
gboolean
ldm_user_get_logged_in (LdmUser *user)
{
    return user->priv->logged_in;
}

static void
ldm_user_init (LdmUser *user)
{
    user->priv = G_TYPE_INSTANCE_GET_PRIVATE (user, LDM_TYPE_USER, LdmUserPrivate);
}

static void
ldm_user_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    LdmUser *self;
    gint i, n_pages;

    self = LDM_USER (object);

    switch (prop_id) {
    case PROP_GREETER:
        self->priv->greeter = g_object_ref (g_value_get_object (value));
        break;
    case PROP_NAME:
        self->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_REAL_NAME:
        self->priv->real_name = g_strdup (g_value_get_string (value));
        break;
    case PROP_IMAGE:
        self->priv->image = g_strdup (g_value_get_string (value));
        break;
    case PROP_LOGGED_IN:
        self->priv->logged_in = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ldm_user_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    LdmUser *self;

    self = LDM_USER (object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, ldm_user_get_name (self));
        break;
    case PROP_REAL_NAME:
        g_value_set_string (value, ldm_user_get_real_name (self));
        break;
    case PROP_DISPLAY_NAME:
        g_value_set_string (value, ldm_user_get_display_name (self));
        break;
    case PROP_IMAGE:
        g_value_set_string (value, ldm_user_get_image (self));
        break;
    case PROP_LANGUAGE:
        g_value_set_string (value, ldm_user_get_language (self));
        break;
    case PROP_LAYOUT:
        g_value_set_string (value, ldm_user_get_layout (self));
        break;
    case PROP_SESSION:
        g_value_set_string (value, ldm_user_get_session (self));
        break;
    case PROP_LOGGED_IN:
        g_value_set_boolean (value, ldm_user_get_logged_in (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ldm_user_class_init (LdmUserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
    g_type_class_add_private (klass, sizeof (LdmUserPrivate));

    object_class->set_property = ldm_user_set_property;
    object_class->get_property = ldm_user_get_property;

    g_object_class_install_property(object_class,
                                    PROP_GREETER,
                                    g_param_spec_object("greeter",
                                                        "greeter",
                                                        "Greeter",
                                                        LDM_TYPE_GREETER,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(object_class,
                                    PROP_NAME,
                                    g_param_spec_string("name",
                                                        "name",
                                                        "Username",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(object_class,
                                    PROP_REAL_NAME,
                                    g_param_spec_string("real-name",
                                                        "real-name",
                                                        "Users real name",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(object_class,
                                    PROP_DISPLAY_NAME,
                                    g_param_spec_string("display-name",
                                                        "display-name",
                                                        "Users display name",
                                                        NULL,
                                                        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
                                    PROP_IMAGE,
                                    g_param_spec_string("image",
                                                        "image",
                                                        "Avatar image",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(object_class,
                                    PROP_LANGUAGE,
                                    g_param_spec_string("language",
                                                        "language",
                                                        "Language used by this user",
                                                        NULL,
                                                        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
                                    PROP_LAYOUT,
                                    g_param_spec_string("layout",
                                                        "layout",
                                                        "Keyboard layout used by this user",
                                                        NULL,
                                                        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
                                    PROP_SESSION,
                                    g_param_spec_string("session",
                                                        "session",
                                                        "Session used by this user",
                                                        NULL,
                                                        G_PARAM_READABLE));
    g_object_class_install_property(object_class,
                                    PROP_LOGGED_IN,
                                    g_param_spec_boolean("logged-in",
                                                         "logged-in",
                                                         "TRUE if the user is currently in a session",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}
