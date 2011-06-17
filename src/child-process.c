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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#include <glib/gstdio.h>

#include "child-process.h"

enum {
    GOT_DATA,
    GOT_SIGNAL,  
    EXITED,
    TERMINATED,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

struct ChildProcessPrivate
{  
    /* Environment variables */
    GHashTable *env;

    /* User to run as */
    User *user;

    /* Path of file to log to */
    gchar *log_file;
  
    /* Pipes to communicate with */
    GIOChannel *to_child_channel;
    GIOChannel *from_child_channel;

    /* Process ID */
    GPid pid;
};

G_DEFINE_TYPE (ChildProcess, child_process, G_TYPE_OBJECT);

static ChildProcess *parent_process = NULL;
static GHashTable *processes = NULL;
static int signal_pipe[2];
static gboolean stopping = FALSE;

ChildProcess *
child_process_get_parent (void)
{
    if (parent_process)
        return parent_process;

    parent_process = child_process_new ();
    parent_process->priv->pid = getpid ();

    return parent_process;
}

ChildProcess *
child_process_new (void)
{
    return g_object_new (CHILD_PROCESS_TYPE, NULL);
}

void
child_process_set_log_file (ChildProcess *process, const gchar *log_file)
{
    g_return_if_fail (process != NULL);

    g_free (process->priv->log_file);
    process->priv->log_file = g_strdup (log_file);
}

const gchar *
child_process_get_log_file (ChildProcess *process)
{
    g_return_val_if_fail (process != NULL, NULL);
    return process->priv->log_file;
}
  
void
child_process_set_env (ChildProcess *process, const gchar *name, const gchar *value)
{
    g_return_if_fail (process != NULL);
    if (value)
        g_hash_table_insert (process->priv->env, g_strdup (name), g_strdup (value));
    else
        g_hash_table_remove (process->priv->env, name);
}

static void
child_process_watch_cb (GPid pid, gint status, gpointer data)
{
    ChildProcess *process = data;

    if (WIFEXITED (status))
    {
        g_debug ("Process %d exited with return value %d", pid, WEXITSTATUS (status));
        g_signal_emit (process, signals[EXITED], 0, WEXITSTATUS (status));
    }
    else if (WIFSIGNALED (status))
    {
        g_debug ("Process %d terminated with signal %d", pid, WTERMSIG (status));
        g_signal_emit (process, signals[TERMINATED], 0, WTERMSIG (status));
    }

    process->priv->pid = 0;
    g_hash_table_remove (processes, GINT_TO_POINTER (pid));

    /* Stop when all processes quit */
    if (stopping && g_hash_table_size (processes) == 0)
        exit (EXIT_SUCCESS);
}

static void
run_child_process (ChildProcess *process, char *const argv[])
{
    GHashTableIter iter;
    gpointer key, value;
    int fd;

    /* FIXME: Close existing file descriptors */

    /* Make input non-blocking */
    fd = g_open ("/dev/null", O_RDONLY);
    dup2 (fd, STDIN_FILENO);
    close (fd);

    /* Set environment */
    g_hash_table_iter_init (&iter, process->priv->env);
    while (g_hash_table_iter_next (&iter, &key, &value))
        g_setenv ((gchar *)key, (gchar *)value, TRUE);

    /* Clear signal handlers so we don't handle them here */
    signal (SIGTERM, SIG_IGN);
    signal (SIGINT, SIG_IGN);
    signal (SIGHUP, SIG_IGN);
    signal (SIGUSR1, SIG_IGN);
    signal (SIGUSR2, SIG_IGN);

    /* Make this process its own session so */
    if (setsid () < 0)
        g_warning ("Failed to make process a new session: %s", strerror (errno));

    if (process->priv->user)
    {
        if (getuid () == 0)
        {
            if (initgroups (user_get_name (process->priv->user), user_get_gid (process->priv->user)) < 0)
            {
                g_warning ("Failed to initialize supplementary groups for %s: %s", user_get_name (process->priv->user), strerror (errno));
                _exit (EXIT_FAILURE);
            }

            if (setgid (user_get_gid (process->priv->user)) != 0)
            {
                g_warning ("Failed to set group ID to %d: %s", user_get_gid (process->priv->user), strerror (errno));
                _exit (EXIT_FAILURE);
            }

            if (setuid (user_get_uid (process->priv->user)) != 0)
            {
                g_warning ("Failed to set user ID to %d: %s", user_get_uid (process->priv->user), strerror (errno));
                _exit (EXIT_FAILURE);
            }
        }

        if (chdir (user_get_home_directory (process->priv->user)) != 0)
        {
            g_warning ("Failed to change to home directory %s: %s", user_get_home_directory (process->priv->user), strerror (errno));
            _exit (EXIT_FAILURE);
        }
    }
  
    /* Redirect output to logfile */
    if (process->priv->log_file)
    {
         int fd;

         fd = g_open (process->priv->log_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
         if (fd < 0)
             g_warning ("Failed to open log file %s: %s", process->priv->log_file, g_strerror (errno));
         else
         {
             dup2 (fd, STDOUT_FILENO);
             dup2 (fd, STDERR_FILENO);
             close (fd);
         }
    }

    execvp (argv[0], argv);

    g_warning ("Error executing child process %s: %s", argv[0], g_strerror (errno));
    _exit (EXIT_FAILURE);
}

static gboolean
from_child_cb (GIOChannel *source, GIOCondition condition, gpointer data)
{
    ChildProcess *process = data;

    if (condition == G_IO_HUP)
    {
        g_debug ("Process %d closed communication channel", process->priv->pid);
        return FALSE;
    }

    g_signal_emit (process, signals[GOT_DATA], 0);
    return TRUE;
}

gboolean
child_process_start (ChildProcess *process,
                     User *user,
                     const gchar *working_dir,
                     const gchar *command,
                     gboolean create_pipe,
                     GError **error)
{
    gboolean result;
    gint argc;
    gchar **argv;
    GString *string;
    gpointer key, value;
    GHashTableIter iter;
    pid_t pid;
    int from_server_fd = -1, to_server_fd = -1;

    g_return_val_if_fail (process != NULL, FALSE);
    g_return_val_if_fail (process->priv->pid == 0, FALSE);

    process->priv->user = g_object_ref (user);

    /* Create the log file owned by the target user */
    if (process->priv->log_file)
    {
        gint fd = g_open (process->priv->log_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        close (fd);
        if (getuid () == 0 && chown (process->priv->log_file, user_get_uid (process->priv->user), user_get_gid (process->priv->user)) != 0)
            g_warning ("Failed to set process log file ownership: %s", strerror (errno));
    }

    if (create_pipe)
    {
        gchar *fd;
        int to_child_pipe[2];
        int from_child_pipe[2];

        if (pipe (to_child_pipe) != 0 || 
            pipe (from_child_pipe) != 0)
        {
            g_warning ("Failed to create pipes: %s", strerror (errno));            
            return FALSE;
        }

        process->priv->to_child_channel = g_io_channel_unix_new (to_child_pipe[1]);
        g_io_channel_set_encoding (process->priv->to_child_channel, NULL, NULL);

        /* Watch for data from the child process */
        process->priv->from_child_channel = g_io_channel_unix_new (from_child_pipe[0]);
        g_io_channel_set_encoding (process->priv->from_child_channel, NULL, NULL);
        g_io_channel_set_buffered (process->priv->from_child_channel, FALSE);

        g_io_add_watch (process->priv->from_child_channel, G_IO_IN | G_IO_HUP, from_child_cb, process);

        to_server_fd = from_child_pipe[1];
        from_server_fd = to_child_pipe[0];

        fd = g_strdup_printf ("%d", to_server_fd);
        child_process_set_env (process, "LDM_TO_SERVER_FD", fd);
        g_free (fd);
        fd = g_strdup_printf ("%d", from_server_fd);
        child_process_set_env (process, "LDM_FROM_SERVER_FD", fd);
        g_free (fd);
    }

    result = g_shell_parse_argv (command, &argc, &argv, error);
    if (!result)
        return FALSE;

    pid = fork ();
    if (pid < 0)
    {
        g_warning ("Failed to fork: %s", strerror (errno));
        return FALSE;
    }

    if (pid == 0)
    {
        /* Close pipes */
        // TEMP: Remove this when have more generic file closing
        if (process->priv->to_child_channel)
            close (g_io_channel_unix_get_fd (process->priv->to_child_channel));
        if (process->priv->from_child_channel)
            close (g_io_channel_unix_get_fd (process->priv->from_child_channel));

        run_child_process (process, argv);
    }
    close (from_server_fd);
    close (to_server_fd);
    g_strfreev (argv);

    string = g_string_new ("");
    g_hash_table_iter_init (&iter, process->priv->env);
    while (g_hash_table_iter_next (&iter, &key, &value))
        g_string_append_printf (string, "%s=%s ", (gchar *)key, (gchar *)value);
    g_string_append (string, command);
    g_debug ("Launching process %d: %s", pid, string->str);
    g_string_free (string, TRUE);

    process->priv->pid = pid;

    g_hash_table_insert (processes, GINT_TO_POINTER (process->priv->pid), g_object_ref (process));
    g_child_watch_add (process->priv->pid, child_process_watch_cb, process);

    return TRUE;
}

GPid
child_process_get_pid (ChildProcess *process)
{
    g_return_val_if_fail (process != NULL, 9);
    return process->priv->pid;
}

void
child_process_signal (ChildProcess *process, int signum)
{
    g_return_if_fail (process != NULL);

    if (process->priv->pid == 0)
        return;

    g_debug ("Sending signal %d to process %d", signum, process->priv->pid);

    if (kill (process->priv->pid, signum) < 0)
        g_warning ("Error sending signal %d to process %d: %s", signum, process->priv->pid, strerror (errno));
}

GIOChannel *
child_process_get_to_child_channel (ChildProcess *process)
{
    g_return_val_if_fail (process != NULL, NULL);
    return process->priv->to_child_channel;
}

GIOChannel *
child_process_get_from_child_channel (ChildProcess *process)
{
    g_return_val_if_fail (process != NULL, NULL);
    return process->priv->from_child_channel;
}

void
child_process_stop_all (void)
{
    GHashTableIter iter;
    gpointer key, value;

    stopping = TRUE;

    /* If no processes, then just quit */
    if (g_hash_table_size (processes) == 0)
        exit (EXIT_SUCCESS);

    g_hash_table_iter_init (&iter, processes);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        ChildProcess *process = (ChildProcess *)value;
        child_process_signal (process, SIGTERM);
    }
}

static void
child_process_init (ChildProcess *process)
{
    process->priv = G_TYPE_INSTANCE_GET_PRIVATE (process, CHILD_PROCESS_TYPE, ChildProcessPrivate);
    process->priv->env = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
child_process_finalize (GObject *object)
{
    ChildProcess *self;

    self = CHILD_PROCESS (object);
  
    if (self->priv->user)
        g_object_unref (self->priv->user);

    if (self->priv->pid > 0)
        g_hash_table_remove (processes, GINT_TO_POINTER (self->priv->pid));

    if (self->priv->pid)
        kill (self->priv->pid, SIGTERM);

    g_hash_table_unref (self->priv->env);

    G_OBJECT_CLASS (child_process_parent_class)->finalize (object);
}

static void
signal_cb (int signum, siginfo_t *info, void *data)
{
    /* NOTE: Using g_printerr as can't call g_warning from a signal callback */
    if (write (signal_pipe[1], &info->si_signo, sizeof (int)) < 0 ||
        write (signal_pipe[1], &info->si_pid, sizeof (pid_t)) < 0)
        g_printerr ("Failed to write to signal pipe: %s", strerror (errno));
}

static gboolean
handle_signal (GIOChannel *source, GIOCondition condition, gpointer data)
{
    int signo;
    pid_t pid;
    ChildProcess *process;

    if (read (signal_pipe[0], &signo, sizeof (int)) < 0 || 
        read (signal_pipe[0], &pid, sizeof (pid_t)) < 0)
    {
        g_warning ("Error reading from signal pipe: %s", strerror (errno));
        return TRUE;
    }

    g_debug ("Got signal %d from process %d", signo, pid);

    process = g_hash_table_lookup (processes, GINT_TO_POINTER (pid));
    if (process == NULL)
        process = child_process_get_parent ();
    if (process)
        g_signal_emit (process, signals[GOT_SIGNAL], 0, signo);

    return TRUE;
}

static void
child_process_class_init (ChildProcessClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    struct sigaction action;

    object_class->finalize = child_process_finalize;  

    g_type_class_add_private (klass, sizeof (ChildProcessPrivate));

    signals[GOT_DATA] =
        g_signal_new ("got-data",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ChildProcessClass, got_data),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0); 
    signals[GOT_SIGNAL] =
        g_signal_new ("got-signal",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ChildProcessClass, got_signal),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1, G_TYPE_INT);
    signals[EXITED] =
        g_signal_new ("exited",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ChildProcessClass, exited),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1, G_TYPE_INT);
    signals[TERMINATED] =
        g_signal_new ("terminated",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ChildProcessClass, terminated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1, G_TYPE_INT);

    /* Catch signals and feed them to the main loop via a pipe */
    processes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
    if (pipe (signal_pipe) != 0)
        g_critical ("Failed to create signal pipe");
    g_io_add_watch (g_io_channel_unix_new (signal_pipe[0]), G_IO_IN, handle_signal, NULL);
    action.sa_sigaction = signal_cb;
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction (SIGTERM, &action, NULL);
    sigaction (SIGINT, &action, NULL);
    sigaction (SIGHUP, &action, NULL);
    sigaction (SIGUSR1, &action, NULL);
    sigaction (SIGUSR2, &action, NULL);
}
