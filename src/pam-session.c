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

#include <string.h>

#include "ldm-marshal.h"
#include "pam-session.h"
#include "user.h"

enum {
    AUTHENTICATION_STARTED,
    STARTED,
    GOT_MESSAGES,
    AUTHENTICATION_RESULT,
    ENDED,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

struct PAMSessionPrivate
{
    /* Service to authenticate against */
    gchar *service;

    /* User being authenticated */
    gchar *username;

    /* Authentication thread */
    GThread *authentication_thread;
  
    /* TRUE if the thread is being intentionally stopped */
    gboolean stop;

    /* TRUE if the conversation should be cancelled */
    gboolean cancel;

    /* Messages requested */
    int num_messages;
    const struct pam_message **messages;
    int result;

    /* Queue to feed responses to the authentication thread */
    GAsyncQueue *authentication_response_queue;

    /* Authentication handle */
    pam_handle_t *pam_handle;
  
    /* TRUE if in a session */
    gboolean in_session;
};

G_DEFINE_TYPE (PAMSession, pam_session, G_TYPE_OBJECT);

static gchar *passwd_file = NULL;

void
pam_session_set_use_pam (void)
{
    pam_session_set_use_passwd_file (NULL);
}

void
pam_session_set_use_passwd_file (gchar *passwd_file_)
{
    g_free (passwd_file);
    passwd_file = g_strdup (passwd_file_);
}

PAMSession *
pam_session_new (const gchar *service, const gchar *username)
{
    PAMSession *self = g_object_new (PAM_SESSION_TYPE, NULL);

    self->priv->service = g_strdup (service);
    self->priv->username = g_strdup (username);

    return self;
}

gboolean
pam_session_get_in_session (PAMSession *session)
{
    g_return_val_if_fail (session != NULL, FALSE);
    return session->priv->in_session;
}

void
pam_session_authorize (PAMSession *session)
{
    g_return_if_fail (session != NULL);

    session->priv->in_session = TRUE;

    if (!passwd_file)
    {
        int result;

        // FIXME:-
        //pam_set_item (session->priv->pam_handle, PAM_TTY, &tty);
        //pam_set_item (session->priv->pam_handle, PAM_XDISPLAY, &display);
        result = pam_open_session (session->priv->pam_handle, 0);
        g_debug ("pam_open_session -> %s", pam_strerror (session->priv->pam_handle, result));

        result = pam_setcred (session->priv->pam_handle, PAM_ESTABLISH_CRED);
        g_debug ("pam_setcred(PAM_ESTABLISH_CRED) -> %s", pam_strerror (session->priv->pam_handle, result));
    }

    g_signal_emit (G_OBJECT (session), signals[STARTED], 0);
}

static gboolean
notify_messages_cb (gpointer data)
{
    PAMSession *session = data;

    g_signal_emit (G_OBJECT (session), signals[GOT_MESSAGES], 0, session->priv->num_messages, session->priv->messages);

    return FALSE;
}

static int
pam_conv_cb (int num_msg, const struct pam_message **msg,
             struct pam_response **resp, void *app_data)
{
    PAMSession *session = app_data;
    struct pam_response *response;

    /* Notify user */
    session->priv->num_messages = num_msg;
    session->priv->messages = msg;
    g_idle_add (notify_messages_cb, session);

    /* Wait for response */
    response = g_async_queue_pop (session->priv->authentication_response_queue);
    session->priv->num_messages = 0;
    session->priv->messages = NULL;

    /* Cancelled by user */
    if (session->priv->stop || session->priv->cancel)
    {
        session->priv->cancel = FALSE;
        return PAM_CONV_ERR;
    }

    *resp = response;

    return PAM_SUCCESS;
}

static gboolean
notify_auth_complete_cb (gpointer data)
{
    PAMSession *session = data;
    int result;

    result = session->priv->result;
    session->priv->result = 0;

    g_thread_join (session->priv->authentication_thread);
    session->priv->authentication_thread = NULL;
    g_async_queue_unref (session->priv->authentication_response_queue);
    session->priv->authentication_response_queue = NULL;

    /* Authentication was cancelled */
    if (session->priv->stop)
        pam_session_end (session);
    else
        g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, result);

    /* The thread is complete, drop the reference */
    g_object_unref (session);

    return FALSE;
}

static gpointer
authenticate_cb (gpointer data)
{
    PAMSession *session = data;
    struct pam_conv conversation = { pam_conv_cb, session };

    pam_start (session->priv->service, session->priv->username, &conversation, &session->priv->pam_handle);

    session->priv->result = pam_authenticate (session->priv->pam_handle, 0);
    g_debug ("pam_authenticate -> %s", pam_strerror (session->priv->pam_handle, session->priv->result));
  
    if (session->priv->result == PAM_SUCCESS)
    {
        session->priv->result = pam_acct_mgmt (session->priv->pam_handle, 0);
        g_debug ("pam_acct_mgmt -> %s", pam_strerror (session->priv->pam_handle, session->priv->result));

        if (session->priv->result == PAM_NEW_AUTHTOK_REQD)
        {
            session->priv->result = pam_chauthtok (session->priv->pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
            g_debug ("pam_chauthtok -> %s", pam_strerror (session->priv->pam_handle, session->priv->result));
        }
    }

    /* Notify user */
    g_idle_add (notify_auth_complete_cb, session);

    return NULL;
}

static gchar *
get_password (const gchar *username)
{
    gchar *data = NULL, **lines, *password = NULL;
    gint i;
    GError *error = NULL;

    if (!g_file_get_contents (passwd_file, &data, NULL, &error))
        g_warning ("Error loading passwd file: %s", error->message);
    g_clear_error (&error);

    if (!data)
        return NULL;

    lines = g_strsplit (data, "\n", -1);
    g_free (data);

    for (i = 0; lines[i] && password == NULL; i++)
    {
        gchar *line, **fields;

        line = g_strstrip (lines[i]);
        fields = g_strsplit (line, ":", -1);
        if (g_strv_length (fields) == 7 && strcmp (fields[0], username) == 0)
            password = g_strdup (fields[1]);
        g_strfreev (fields);
    }
    g_strfreev (lines);

    return password;
}

static void
send_message (PAMSession *session, gint style, const gchar *text)
{
    struct pam_message **messages;

    messages = calloc (1, sizeof (struct pam_message *));
    messages[0] = g_malloc0 (sizeof (struct pam_message));
    messages[0]->msg_style = style;
    messages[0]->msg = g_strdup (text);
    session->priv->messages = (const struct pam_message **) messages;
    session->priv->num_messages = 1;

    g_signal_emit (G_OBJECT (session), signals[GOT_MESSAGES], 0, session->priv->num_messages, session->priv->messages);
}

gboolean
pam_session_start (PAMSession *session, GError **error)
{
    g_return_val_if_fail (session != NULL, FALSE);
    g_return_val_if_fail (session->priv->authentication_thread == NULL, FALSE);

    g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_STARTED], 0);

    if (passwd_file)
    {
        if (session->priv->username == NULL)
            send_message (session, PAM_PROMPT_ECHO_ON, "login:");
        else
        {
            gchar *password;

            password = get_password (session->priv->username);
            /* Always succeed with autologin, or no password on account otherwise prompt for a password */
            if (strcmp (session->priv->service, "lightdm-autologin") == 0 ||
                g_strcmp0 (password, "") == 0)
                g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, PAM_SUCCESS);
            else
                send_message (session, PAM_PROMPT_ECHO_OFF, "Password:");
            g_free (password);
        }      
    }
    else
    {
        /* Hold a reference to this object while the thread may access it */
        g_object_ref (session);

        /* Start thread */
        session->priv->authentication_response_queue = g_async_queue_new ();
        session->priv->authentication_thread = g_thread_create (authenticate_cb, session, TRUE, error);
        if (!session->priv->authentication_thread)
            return FALSE;
    }

    return TRUE;
}

const gchar *
pam_session_strerror (PAMSession *session, int error)
{
    g_return_val_if_fail (session != NULL, NULL);
    return pam_strerror (session->priv->pam_handle, error);
}

const gchar *
pam_session_get_username (PAMSession *session)
{
    const char *username;

    g_return_val_if_fail (session != NULL, NULL);

    if (session->priv->pam_handle)
    {
        g_free (session->priv->username);
        pam_get_item (session->priv->pam_handle, PAM_USER, (const void **) &username);
        session->priv->username = g_strdup (username);
    }

    return session->priv->username;
}

const struct pam_message **
pam_session_get_messages (PAMSession *session)
{
    g_return_val_if_fail (session != NULL, NULL);
    return session->priv->messages;  
}

gint
pam_session_get_num_messages (PAMSession *session)
{
    g_return_val_if_fail (session != NULL, 0);
    return session->priv->num_messages;
}

void
pam_session_respond (PAMSession *session, struct pam_response *response)
{
    g_return_if_fail (session != NULL);

    if (passwd_file)
    {
        gchar *password;

        if (session->priv->messages)
        {
            int i;
            struct pam_message **messages = (struct pam_message **) session->priv->messages;

            for (i = 0; i < session->priv->num_messages; i++)
            {
                g_free ((gchar *) messages[i]->msg);
                g_free (messages[i]);
            }
            g_free (messages);
            session->priv->messages = NULL;
            session->priv->num_messages = 0;
        }


        if (session->priv->username == NULL)
        {
            session->priv->username = g_strdup (response->resp);
            password = get_password (session->priv->username);
            if (g_strcmp0 (password, "") == 0)
                g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, PAM_SUCCESS);
            else
                send_message (session, PAM_PROMPT_ECHO_OFF, "Password:");
        }
        else
        {
            User *user;

            user = user_get_by_name (session->priv->username);
            password = get_password (session->priv->username);
            if (user && g_strcmp0 (response->resp, password) == 0)
                g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, PAM_SUCCESS);
            else
                g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, PAM_AUTH_ERR);

            if (user)
                g_object_unref (user);
        }
        g_free (password);
        g_free (response->resp);
        g_free (response);
    }
    else
    {
        g_return_if_fail (session->priv->authentication_thread != NULL);
        g_async_queue_push (session->priv->authentication_response_queue, response);
    }
}

void
pam_session_cancel (PAMSession *session)
{
    g_return_if_fail (session != NULL);

    if (!passwd_file)
    {
        g_signal_emit (G_OBJECT (session), signals[AUTHENTICATION_RESULT], 0, PAM_CONV_ERR);
    }
    else
    {
        g_return_if_fail (session->priv->authentication_thread != NULL);
        session->priv->cancel = TRUE;
        g_async_queue_push (session->priv->authentication_response_queue, GINT_TO_POINTER (-1));
    }
}

const gchar *
pam_session_getenv (PAMSession *session, const gchar *name)
{
    g_return_val_if_fail (session != NULL, NULL);
    if (passwd_file)
        return NULL;
    else
        return pam_getenv (session->priv->pam_handle, name);
}

gchar **
pam_session_get_envlist (PAMSession *session)
{
    g_return_val_if_fail (session != NULL, NULL);
    if (passwd_file)
    {
        char **env_list = calloc (1, sizeof (gchar *));
        env_list[0] = NULL;
        return env_list;
    }
    else
        return pam_getenvlist (session->priv->pam_handle);
}

void
pam_session_end (PAMSession *session)
{
    g_return_if_fail (session != NULL);

    /* If authenticating cancel first */
    if (session->priv->authentication_thread)
    {
        session->priv->stop = TRUE;
        g_async_queue_push (session->priv->authentication_response_queue, GINT_TO_POINTER (-1));
    }
    else if (session->priv->in_session)
    {
        int result;

        result = pam_close_session (session->priv->pam_handle, 0);
        g_debug ("pam_close_session -> %s", pam_strerror (session->priv->pam_handle, result));

        pam_setcred (session->priv->pam_handle, PAM_DELETE_CRED);
        g_debug ("pam_setcred(PAM_DELETE_CRED) -> %s", pam_strerror (session->priv->pam_handle, result));

        session->priv->in_session = FALSE;
        g_signal_emit (G_OBJECT (session), signals[ENDED], 0);
    }
}

static void
pam_session_init (PAMSession *session)
{
    session->priv = G_TYPE_INSTANCE_GET_PRIVATE (session, PAM_SESSION_TYPE, PAMSessionPrivate);
}

static void
pam_session_finalize (GObject *object)
{
    PAMSession *self;

    self = PAM_SESSION (object);
  
    pam_session_end (self);

    g_free (self->priv->username);
    if (self->priv->pam_handle)
        pam_end (self->priv->pam_handle, PAM_SUCCESS);
  
    G_OBJECT_CLASS (pam_session_parent_class)->finalize (object);
}

static void
pam_session_class_init (PAMSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = pam_session_finalize;  

    g_type_class_add_private (klass, sizeof (PAMSessionPrivate));

    signals[AUTHENTICATION_STARTED] =
        g_signal_new ("authentication-started",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PAMSessionClass, authentication_started),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[STARTED] =
        g_signal_new ("started",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PAMSessionClass, started),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[GOT_MESSAGES] =
        g_signal_new ("got-messages",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PAMSessionClass, got_messages),
                      NULL, NULL,
                      ldm_marshal_VOID__INT_POINTER,
                      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_POINTER);

    signals[AUTHENTICATION_RESULT] =
        g_signal_new ("authentication-result",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PAMSessionClass, authentication_result),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1, G_TYPE_INT);

    signals[ENDED] =
        g_signal_new ("ended",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PAMSessionClass, ended),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}
