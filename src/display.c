/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 * 
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gdesktopappinfo.h>

#include "display.h"
#include "configuration.h"
#include "user.h"
#include "pam-session.h"
#include "dmrc.h"
#include "theme.h"
#include "ldm-marshal.h"
#include "greeter.h"

/* Length of time in milliseconds to wait for a session to load */
#define USER_SESSION_TIMEOUT 5000

enum {
    START_GREETER,
    END_GREETER,
    START_SESSION,
    END_SESSION,
    EXITED,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

typedef enum
{
    SESSION_NONE = 0,
    SESSION_GREETER_PRE_CONNECT,
    SESSION_GREETER,
    SESSION_GREETER_AUTHENTICATED,
    SESSION_USER
} SessionType;

struct DisplayPrivate
{
    gint index;

    /* X server */
    XServer *xserver;

    /* Number of times we have shown the greeter */
    gint greeter_count;
  
    /* Number of times have logged in */
    gint login_count;

    /* User to run greeter as */
    gchar *greeter_user;

    /* Theme to use */
    gchar *greeter_theme;

    /* Program to run sessions through */
    gchar *session_wrapper;

    /* PAM service to authenticate against */
    gchar *pam_service;

    /* PAM service to authenticate against for automatic logins */
    gchar *pam_autologin_service;

    /* Greeter session process */
    Greeter *greeter_session;
    PAMSession *greeter_pam_session;
    gchar *greeter_ck_cookie;

    /* TRUE if the greeter can stay active during the session */
    gboolean supports_transitions;

    /* User session process */
    Session *user_session;
    guint user_session_timer;
    PAMSession *user_pam_session;
    gchar *user_ck_cookie;

    /* Default login hint */
    gchar *default_user;
    gint timeout;

    /* Default session */
    gchar *default_session;
};

G_DEFINE_TYPE (Display, display, G_TYPE_OBJECT);

static gboolean start_greeter (Display *display);

// FIXME: Remove the index, it is an external property
Display *
display_new (gint index)
{
    Display *self = g_object_new (DISPLAY_TYPE, NULL);

    self->priv->index = index;
    self->priv->pam_service = g_strdup (DEFAULT_PAM_SERVICE);
    self->priv->pam_autologin_service = g_strdup (DEFAULT_PAM_AUTOLOGIN_SERVICE);

    return self;
}

gint
display_get_index (Display *display)
{
    g_return_val_if_fail (display != NULL, 0);
    return display->priv->index;
}

void
display_set_session_wrapper (Display *display, const gchar *session_wrapper)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->session_wrapper);
    display->priv->session_wrapper = g_strdup (session_wrapper);  
}

const gchar *
display_get_session_wrapper (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->session_wrapper;
}

void
display_set_default_user (Display *display, const gchar *username)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->default_user);
    display->priv->default_user = g_strdup (username);
}

const gchar *
display_get_default_user (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->default_user;
}

void
display_set_default_user_timeout (Display *display, gint timeout)
{
    g_return_if_fail (display != NULL);
    display->priv->timeout = timeout;  
}

gint
display_get_default_user_timeout (Display *display)
{
    g_return_val_if_fail (display != NULL, 0);
    return display->priv->timeout;
}

void
display_set_greeter_user (Display *display, const gchar *username)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->greeter_user);
    if (username && username[0] != '\0')
        display->priv->greeter_user = g_strdup (username);
    else
        display->priv->greeter_user = NULL;
}

const gchar *
display_get_greeter_user (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->greeter_user;  
}

const gchar *
display_get_session_user (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);

    if (display->priv->user_session)
        return pam_session_get_username (display->priv->user_pam_session);
    else
        return NULL;
}

void
display_set_greeter_theme (Display *display, const gchar *greeter_theme)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->greeter_theme);
    display->priv->greeter_theme = g_strdup (greeter_theme);
}

const gchar *
display_get_greeter_theme (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->greeter_theme;
}

void
display_set_default_session (Display *display, const gchar *session)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->default_session);
    display->priv->default_session = g_strdup (session);
}

const gchar *
display_get_default_session (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->default_session;
}

void
display_set_pam_service (Display *display, const gchar *service)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->pam_service);
    display->priv->pam_service = g_strdup (service);
}

const gchar *
display_get_pam_service (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->pam_service;
}

void
display_set_pam_autologin_service (Display *display, const gchar *service)
{
    g_return_if_fail (display != NULL);

    g_free (display->priv->pam_autologin_service);
    display->priv->pam_autologin_service = g_strdup (service);
}

const gchar *
display_get_pam_autologin_service (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->pam_autologin_service;
}

void
display_set_xserver (Display *display, XServer *xserver)
{
    g_return_if_fail (display != NULL);

    if (display->priv->xserver)
        g_object_unref (display->priv->xserver);
    display->priv->xserver = g_object_ref (xserver);
}

XServer *
display_get_xserver (Display *display)
{
    g_return_val_if_fail (display != NULL, NULL);
    return display->priv->xserver;
}

static gchar *
start_ck_session (Display *display, const gchar *session_type, User *user)
{
    GDBusProxy *proxy;
    char *display_device = NULL;
    const gchar *address, *hostname = "";
    GVariantBuilder arg_builder;
    GVariant *result;
    gchar *cookie = NULL;
    GError *error = NULL;

    /* Only start ConsoleKit sessions when running as root */
    if (getuid () != 0)
    {
        g_debug ("Not opening ConsoleKit session - not running as root");
        return NULL;
    }

    if (xserver_get_vt (display->priv->xserver) >= 0)
        display_device = g_strdup_printf ("/dev/tty%d", xserver_get_vt (display->priv->xserver));
    address = xserver_get_address (display->priv->xserver);

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           NULL,
                                           "org.freedesktop.ConsoleKit",
                                           "/org/freedesktop/ConsoleKit/Manager",
                                           "org.freedesktop.ConsoleKit.Manager", 
                                           NULL, &error);
    if (!proxy)
        g_warning ("Unable to get connection to ConsoleKit: %s", error->message);
    g_clear_error (&error);
    if (!proxy)
        return NULL;

    g_variant_builder_init (&arg_builder, G_VARIANT_TYPE ("(a(sv))"));
    g_variant_builder_open (&arg_builder, G_VARIANT_TYPE ("a(sv)"));
    g_variant_builder_add (&arg_builder, "(sv)", "unix-user", g_variant_new_int32 (user_get_uid (user)));
    g_variant_builder_add (&arg_builder, "(sv)", "session-type", g_variant_new_string (session_type));
    g_variant_builder_add (&arg_builder, "(sv)", "x11-display", g_variant_new_string (address));
    if (display_device)
        g_variant_builder_add (&arg_builder, "(sv)", "x11-display-device", g_variant_new_string (display_device));
    g_variant_builder_add (&arg_builder, "(sv)", "remote-host-name", g_variant_new_string (hostname));
    g_variant_builder_add (&arg_builder, "(sv)", "is-local", g_variant_new_boolean (TRUE));
    g_variant_builder_close (&arg_builder);
    g_free (display_device);

    result = g_dbus_proxy_call_sync (proxy,
                                     "OpenSessionWithParameters",
                                     g_variant_builder_end (&arg_builder),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);
    g_object_unref (proxy);

    if (!result)
        g_warning ("Failed to open CK session: %s", error->message);
    g_clear_error (&error);
    if (!result)
        return NULL;

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(s)")))
        g_variant_get (result, "(s)", &cookie);
    else
        g_warning ("Unexpected response from OpenSessionWithParameters: %s", g_variant_get_type_string (result));
    g_variant_unref (result);

    if (cookie)
        g_debug ("Opened ConsoleKit session %s", cookie);

    return cookie;
}

static void
end_ck_session (const gchar *cookie)
{
    GDBusProxy *proxy;
    GVariant *result;
    GError *error = NULL;

    if (!cookie)
        return;

    g_debug ("Ending ConsoleKit session %s", cookie);

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           NULL,
                                           "org.freedesktop.ConsoleKit",
                                           "/org/freedesktop/ConsoleKit/Manager",
                                           "org.freedesktop.ConsoleKit.Manager", 
                                           NULL, NULL);
    result = g_dbus_proxy_call_sync (proxy,
                                     "CloseSession",
                                     g_variant_new ("(s)", cookie),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);
    g_object_unref (proxy);

    if (!result)
        g_warning ("Error ending ConsoleKit session: %s", error->message);
    g_clear_error (&error);
    if (!result)
        return;

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(b)")))
    {
        gboolean is_closed;
        g_variant_get (result, "(b)", &is_closed);
        if (!is_closed)
            g_warning ("ConsoleKit.Manager.CloseSession() returned false");
    }
    else
        g_warning ("Unexpected response from CloseSession: %s", g_variant_get_type_string (result));

    g_variant_unref (result);
}

static void
run_script (const gchar *script)
{
    // FIXME
}

static void
end_user_session (Display *display, gboolean clean_exit)
{
    run_script ("PostSession");

    g_signal_emit (display, signals[END_SESSION], 0, display->priv->user_session);
  
    if (display->priv->user_session_timer)
    {
        g_source_remove (display->priv->user_session_timer);
        display->priv->user_session_timer = 0;
    }

    g_object_unref (display->priv->user_session);
    display->priv->user_session = NULL;

    pam_session_end (display->priv->user_pam_session);
    g_object_unref (display->priv->user_pam_session);
    display->priv->user_pam_session = NULL;

    end_ck_session (display->priv->user_ck_cookie);
    g_free (display->priv->user_ck_cookie);
    display->priv->user_ck_cookie = NULL;

    if (!clean_exit)
        g_debug ("Session exited unexpectedly");

    if (display->priv->xserver)
        xserver_disconnect_clients (display->priv->xserver);
}

static void
user_session_exited_cb (Session *session, gint status, Display *display)
{
    end_user_session (display, status == 0);
}

static void
user_session_terminated_cb (Session *session, gint signum, Display *display)
{
    end_user_session (display, FALSE);
}

static void
set_env_from_pam_session (Session *session, PAMSession *pam_session)
{
    gchar **pam_env;

    pam_env = pam_session_get_envlist (pam_session);
    if (pam_env)
    {
        gchar *env_string;      
        int i;

        env_string = g_strjoinv (" ", pam_env);
        g_debug ("PAM returns environment '%s'", env_string);
        g_free (env_string);

        for (i = 0; pam_env[i]; i++)
        {
            gchar **pam_env_vars = g_strsplit (pam_env[i], "=", 2);
            if (pam_env_vars && pam_env_vars[0] && pam_env_vars[1])
                child_process_set_env (CHILD_PROCESS (session), pam_env_vars[0], pam_env_vars[1]);
            else
                g_warning ("Can't parse PAM environment variable %s", pam_env[i]);
            g_strfreev (pam_env_vars);
        }
        g_strfreev (pam_env);
    }
}

static void
set_env_from_keyfile (Session *session, const gchar *name, GKeyFile *key_file, const gchar *section, const gchar *key)
{
    char *value;

    value = g_key_file_get_string (key_file, section, key, NULL);
    if (!value)
        return;

    child_process_set_env (CHILD_PROCESS (session), name, value);
    g_free (value);
}

static gboolean
start_user_session (Display *display, const gchar *session, const gchar *language)
{
    gchar *filename, *path, *old_language, *xsessions_dir;
    gchar *session_command;
    User *user;
    gboolean supports_transitions;
    GKeyFile *dmrc_file, *session_desktop_file;
    gboolean result;
    GError *error = NULL;

    run_script ("PreSession");

    g_debug ("Launching '%s' session for user %s", session, pam_session_get_username (display->priv->user_pam_session));
    display->priv->login_count++;

    /* Load the users login settings (~/.dmrc) */
    dmrc_file = dmrc_load (pam_session_get_username (display->priv->user_pam_session));

    /* Update the .dmrc with changed settings */
    g_key_file_set_string (dmrc_file, "Desktop", "Session", session);
    old_language = g_key_file_get_string (dmrc_file, "Desktop", "Language", NULL);
    if (language && (!old_language || !g_str_equal(language, old_language)))
    {
        g_key_file_set_string (dmrc_file, "Desktop", "Language", language);
        /* We don't have advanced language checking, so reset these variables */
        g_key_file_remove_key (dmrc_file, "Desktop", "Langlist", NULL);
        g_key_file_remove_key (dmrc_file, "Desktop", "LCMess", NULL);
    }
    g_free (old_language);

    xsessions_dir = config_get_string (config_get_instance (), "LightDM", "xsessions-directory");
    filename = g_strdup_printf ("%s.desktop", session);
    path = g_build_filename (xsessions_dir, filename, NULL);
    g_free (xsessions_dir);
    g_free (filename);

    session_desktop_file = g_key_file_new ();
    result = g_key_file_load_from_file (session_desktop_file, path, G_KEY_FILE_NONE, &error);
    g_free (path);
    if (!result)
        g_warning ("Failed to load session file %s: %s:", path, error->message);
    g_clear_error (&error);
    if (!result)
        return FALSE;
    session_command = g_key_file_get_string (session_desktop_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
    supports_transitions = g_key_file_get_boolean (session_desktop_file, G_KEY_FILE_DESKTOP_GROUP, "X-LightDM-Supports-Transitions", NULL);
    g_key_file_free (session_desktop_file);

    if (!session_command)
    {
        g_warning ("No command in session file %s", path);
        return FALSE;
    }
    if (display->priv->session_wrapper)
    {
        gchar *old_command = session_command;
        session_command = g_strdup_printf ("%s '%s'", display->priv->session_wrapper, session_command);
        g_free (old_command);
    }

    user = user_get_by_name (pam_session_get_username (display->priv->user_pam_session));
    if (!user)
    {
        g_free (session_command);
        g_warning ("Unable to start session, user %s does not exist", pam_session_get_username (display->priv->user_pam_session));
        return FALSE;
    }

    /* Open ConsoleKit session */
    display->priv->user_ck_cookie = start_ck_session (display, "", user);

    display->priv->supports_transitions = supports_transitions;
    display->priv->user_session = session_new ();

    session_set_user (display->priv->user_session, user);
    g_object_unref (user);

    session_set_command (display->priv->user_session, session_command);
    g_free (session_command);

    g_signal_connect (G_OBJECT (display->priv->user_session), "exited", G_CALLBACK (user_session_exited_cb), display);
    g_signal_connect (G_OBJECT (display->priv->user_session), "terminated", G_CALLBACK (user_session_terminated_cb), display);
    child_process_set_env (CHILD_PROCESS (display->priv->user_session), "DISPLAY", xserver_get_address (display->priv->xserver));
    if (display->priv->user_ck_cookie)
        child_process_set_env (CHILD_PROCESS (display->priv->user_session), "XDG_SESSION_COOKIE", display->priv->user_ck_cookie);
    child_process_set_env (CHILD_PROCESS (display->priv->user_session), "DESKTOP_SESSION", session); // FIXME: Apparently deprecated?
    child_process_set_env (CHILD_PROCESS (display->priv->user_session), "GDMSESSION", session); // FIXME: Not cross-desktop
    set_env_from_keyfile (display->priv->user_session, "LANG", dmrc_file, "Desktop", "Language");
    set_env_from_keyfile (display->priv->user_session, "LANGUAGE", dmrc_file, "Desktop", "Langlist");
    set_env_from_keyfile (display->priv->user_session, "LC_MESSAGES", dmrc_file, "Desktop", "LCMess");
    //child_process_set_env (CHILD_PROCESS (display->priv->user_session), "GDM_LANG", session_language); // FIXME: Not cross-desktop
    set_env_from_keyfile (display->priv->user_session, "GDM_KEYBOARD_LAYOUT", dmrc_file, "Desktop", "Layout"); // FIXME: Not cross-desktop
    set_env_from_pam_session (display->priv->user_session, display->priv->user_pam_session);

    g_signal_emit (display, signals[START_SESSION], 0, display->priv->user_session);

    /* Start it now, or wait for the greeter to quit */
    if (display->priv->greeter_session == NULL || display->priv->supports_transitions)
        result = session_start (display->priv->user_session, FALSE);
    else
    {
        g_debug ("Waiting for greeter to quit before starting user session process");
        result = TRUE;
    }

    /* Save modified DMRC */
    dmrc_save (dmrc_file, pam_session_get_username (display->priv->user_pam_session));
    g_key_file_free (dmrc_file);

    return result;
}

static void
default_session_pam_message_cb (PAMSession *session, int num_msg, const struct pam_message **msg, Display *display)
{
    g_debug ("Aborting automatic login, PAM requests input");
    pam_session_cancel (session);
}

static void
default_session_authentication_result_cb (PAMSession *session, int result, Display *display)
{
    if (result == PAM_SUCCESS)
    {
        g_debug ("User %s authorized", pam_session_get_username (session));
        pam_session_authorize (session);
        start_user_session (display, display->priv->default_session, NULL);
    }
    else
    {
        g_debug ("Failed to authorize default user, starting greeter");
        start_greeter (display);
    }
}
  
static gboolean
start_autologin_session (Display *display, GError **error)
{
    /* Run using autologin PAM session, abort if get asked any questions */
    if (display->priv->user_pam_session)
        pam_session_end (display->priv->user_pam_session);
    display->priv->user_pam_session = pam_session_new (display->priv->pam_autologin_service, display->priv->default_user);
    g_signal_connect (display->priv->user_pam_session, "got-messages", G_CALLBACK (default_session_pam_message_cb), display);
    g_signal_connect (display->priv->user_pam_session, "authentication-result", G_CALLBACK (default_session_authentication_result_cb), display);
    if (!pam_session_start (display->priv->user_pam_session, error))
        return FALSE;

    return TRUE;
}

static gboolean
session_timeout_cb (Display *display)
{
    g_warning ("Session has not indicated it is ready, stopping greeter anyway");

    /* Stop the greeter */
    greeter_quit (display->priv->greeter_session);

    display->priv->user_session_timer = 0;
    return FALSE;
}

static void
greeter_start_session_cb (Greeter *greeter, const gchar *session, const gchar *language, Display *display)
{
    /* Default session requested */
    if (strcmp (session, "") == 0)
        session = display->priv->default_session;

    /* Default language requested */
    if (strcmp (language, "") == 0)
        language = NULL;
  
    display->priv->user_pam_session = greeter_get_pam_session (greeter);

    if (!display->priv->user_pam_session ||
        !pam_session_get_in_session (display->priv->user_pam_session))
    {
        g_warning ("Ignoring request for login with unauthenticated user");
        return;
    }

    start_user_session (display, session, language);

    /* Stop session, waiting for user session to indicate it is ready (if supported) */
    // FIXME: Hard-coded timeout
    // FIXME: Greeter quit timeout
    if (display->priv->supports_transitions)
        display->priv->user_session_timer = g_timeout_add (USER_SESSION_TIMEOUT, (GSourceFunc) session_timeout_cb, display);
    else
        greeter_quit (display->priv->greeter_session);
}

static void
greeter_quit_cb (Greeter *greeter, Display *display)
{
    g_debug ("Greeter quit");

    g_signal_emit (display, signals[END_GREETER], 0, display->priv->greeter_session);

    pam_session_end (display->priv->greeter_pam_session);
    g_object_unref (display->priv->greeter_pam_session);
    display->priv->greeter_pam_session = NULL;

    g_object_unref (display->priv->greeter_session);
    display->priv->greeter_session = NULL;

    end_ck_session (display->priv->greeter_ck_cookie);
    g_free (display->priv->greeter_ck_cookie);
    display->priv->greeter_ck_cookie = NULL;   

    /* Start session if waiting for greeter to quit */
    if (display->priv->user_session && child_process_get_pid (CHILD_PROCESS (display->priv->user_session)) == 0)
    {
        g_debug ("Starting user session");
        session_start (display->priv->user_session, FALSE);
    }
}

static gboolean
start_greeter (Display *display)
{
    GKeyFile *theme;
    gchar *command;
    User *user;
    gboolean result;
    GError *error = NULL;

    theme = load_theme (display->priv->greeter_theme, &error);
    if (!theme)
        g_warning ("Failed to find theme %s: %s", display->priv->greeter_theme, error->message);
    g_clear_error (&error);
    if (!theme)
        return FALSE;

    if (display->priv->greeter_user)
    {
        user = user_get_by_name (display->priv->greeter_user);
        if (!user)
        {
            g_warning ("Unable to start greeter, user %s does not exist", display->priv->greeter_user);
            return FALSE;
        }
    }
    else
        user = user_get_current ();

    g_debug ("Starting greeter %s as user %s", display->priv->greeter_theme, user_get_name (user));
    display->priv->greeter_count++;

    display->priv->greeter_pam_session = pam_session_new (display->priv->pam_service, user_get_name (user));
    pam_session_authorize (display->priv->greeter_pam_session);

    display->priv->greeter_ck_cookie = start_ck_session (display, "LoginWindow", user);

    display->priv->greeter_session = greeter_new (display->priv->greeter_theme, display->priv->greeter_count - 1);
    greeter_set_default_user (display->priv->greeter_session, display->priv->default_user, display->priv->timeout);
    greeter_set_default_session (display->priv->greeter_session, display->priv->default_session);
    g_signal_connect (G_OBJECT (display->priv->greeter_session), "start-session", G_CALLBACK (greeter_start_session_cb), display);
    g_signal_connect (G_OBJECT (display->priv->greeter_session), "quit", G_CALLBACK (greeter_quit_cb), display);
    session_set_user (SESSION (display->priv->greeter_session), user);
    command = theme_get_command (theme);
    session_set_command (SESSION (display->priv->greeter_session), command);
    g_free (command);
    child_process_set_env (CHILD_PROCESS (display->priv->greeter_session), "DISPLAY", xserver_get_address (display->priv->xserver));
    if (display->priv->greeter_ck_cookie)
        child_process_set_env (CHILD_PROCESS (display->priv->greeter_session), "XDG_SESSION_COOKIE", display->priv->greeter_ck_cookie);
    set_env_from_pam_session (SESSION (display->priv->greeter_session), display->priv->greeter_pam_session);

    g_signal_emit (display, signals[START_GREETER], 0, display->priv->greeter_session);

    result = session_start (SESSION (display->priv->greeter_session), TRUE);

    g_key_file_free (theme);
    g_object_unref (user);

    return result;
}

static void
end_display (Display *display)
{
    g_object_unref (display->priv->xserver);
    display->priv->xserver = NULL;
    g_signal_emit (display, signals[EXITED], 0);
}

static void
xserver_exit_cb (XServer *server, int status, Display *display)
{
    if (status != 0)
        g_debug ("X server exited with value %d", status);
    end_display (display);
}

static void
xserver_terminate_cb (XServer *server, int signum, Display *display)
{
    g_debug ("X server terminated with signal %d", signum);
    end_display (display);
}

static void
xserver_ready_cb (XServer *xserver, Display *display)
{
    run_script ("Init"); // FIXME: Async

    /* Don't run any sessions on local terminals */
    if (xserver_get_server_type (xserver) == XSERVER_TYPE_LOCAL_TERMINAL)
        return;

    /* If have user then automatically login the first time */
    if (display->priv->default_user && display->priv->timeout == 0 && display->priv->login_count == 0)
    {
        GError *error = NULL;
        gboolean result;

        g_debug ("Automatically logging in user %s", display->priv->default_user);
        result = start_autologin_session (display, &error);
        if (!result)
            g_warning ("Failed to autologin user %s, starting greeter instead: %s", display->priv->default_user, error->message);
        g_clear_error (&error);
        if (result)
            return;
    }

    start_greeter (display);
}

gboolean
display_start (Display *display)
{
    g_return_val_if_fail (display != NULL, FALSE);
    g_return_val_if_fail (display->priv->xserver != NULL, FALSE);

    g_signal_connect (G_OBJECT (display->priv->xserver), "ready", G_CALLBACK (xserver_ready_cb), display);
    g_signal_connect (G_OBJECT (display->priv->xserver), "exited", G_CALLBACK (xserver_exit_cb), display);
    g_signal_connect (G_OBJECT (display->priv->xserver), "terminated", G_CALLBACK (xserver_terminate_cb), display);
    return xserver_start (display->priv->xserver);
}

static void
display_init (Display *display)
{
    display->priv = G_TYPE_INSTANCE_GET_PRIVATE (display, DISPLAY_TYPE, DisplayPrivate);
    if (strcmp (GREETER_USER, "") != 0)
        display->priv->greeter_user = g_strdup (GREETER_USER);
    display->priv->greeter_theme = config_get_string (config_get_instance (), "LightDM", "default-greeter-theme");
    display->priv->default_session = config_get_string (config_get_instance (), "LightDM", "default-xsession");
}

static void
display_finalize (GObject *object)
{
    Display *self;

    self = DISPLAY (object);

    if (self->priv->xserver)
        g_object_unref (self->priv->xserver);
    g_free (self->priv->greeter_user);
    g_free (self->priv->greeter_theme);
    g_free (self->priv->session_wrapper);
    g_free (self->priv->pam_service);
    g_free (self->priv->pam_autologin_service);
    if (self->priv->greeter_session)
        g_object_unref (self->priv->greeter_session);
    if (self->priv->greeter_pam_session)
        g_object_unref (self->priv->greeter_pam_session);
    end_ck_session (self->priv->greeter_ck_cookie);
    g_free (self->priv->greeter_ck_cookie);
    if (self->priv->user_session)
        g_object_unref (self->priv->user_session);
    if (self->priv->user_session_timer)
        g_source_remove (self->priv->user_session_timer);
    if (self->priv->user_pam_session)
        g_object_unref (self->priv->user_pam_session);
    end_ck_session (self->priv->user_ck_cookie);
    g_free (self->priv->user_ck_cookie);
    g_free (self->priv->default_user);
    g_free (self->priv->default_session);

    G_OBJECT_CLASS (display_parent_class)->finalize (object);
}

static void
display_class_init (DisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = display_finalize;

    g_type_class_add_private (klass, sizeof (DisplayPrivate));

    signals[START_GREETER] =
        g_signal_new ("start-greeter",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (DisplayClass, start_greeter),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, SESSION_TYPE);

    signals[END_GREETER] =
        g_signal_new ("end-greeter",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (DisplayClass, end_greeter),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, SESSION_TYPE);
  
    signals[START_SESSION] =
        g_signal_new ("start-session",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (DisplayClass, start_session),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, SESSION_TYPE);

    signals[END_SESSION] =
        g_signal_new ("end-session",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (DisplayClass, end_session),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, SESSION_TYPE);

    signals[EXITED] =
        g_signal_new ("exited",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (DisplayClass, exited),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}
