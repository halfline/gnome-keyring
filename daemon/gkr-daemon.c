/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-keyring-daemon.c - main keyring daemon code.

   Copyright (C) 2003 Red Hat, Inc

   Gnome keyring is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   Gnome keyring is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "config.h"

#include "gkr-daemon.h"

#include "common/gkr-async.h"
#include "common/gkr-cleanup.h"
#include "common/gkr-unix-signal.h"

#include "keyrings/gkr-keyrings.h"

#include "library/gnome-keyring.h"
#include "library/gnome-keyring-memory.h"

#include "ui/gkr-ask-daemon.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gcrypt.h>

static GMainLoop *loop = NULL;

#ifndef HAVE_SOCKLEN_T
#define socklen_t int
#endif

static void
log_handler (const gchar *log_domain, GLogLevelFlags log_level, 
             const gchar *message, gpointer user_data)
{
    int level;

    /* Note that crit and err are the other way around in syslog */
        
    switch (G_LOG_LEVEL_MASK & log_level) {
    case G_LOG_LEVEL_ERROR:
        level = LOG_CRIT;
        break;
    case G_LOG_LEVEL_CRITICAL:
        level = LOG_ERR;
        break;
    case G_LOG_LEVEL_WARNING:
        level = LOG_WARNING;
        break;
    case G_LOG_LEVEL_MESSAGE:
        level = LOG_NOTICE;
        break;
    case G_LOG_LEVEL_INFO:
        level = LOG_INFO;
        break;
    case G_LOG_LEVEL_DEBUG:
        level = LOG_DEBUG;
        break;
    default:
        level = LOG_ERR;
        break;
    }
    
    /* Log to syslog first */
    if (log_domain)
        syslog (level, "%s: %s", log_domain, message);
    else
        syslog (level, "%s", message);
 
    /* And then to default handler for aborting and stuff like that */
    g_log_default_handler (log_domain, log_level, message, user_data); 
}

static void
prepare_logging ()
{
    GLogLevelFlags flags = G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR | 
                G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | 
                G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO;
                
    openlog ("gnome-keyring-daemon", LOG_PID, LOG_AUTH);
    
    g_log_set_handler (NULL, flags, log_handler, NULL);
    g_log_set_handler ("Glib", flags, log_handler, NULL);
    g_log_set_handler ("Gtk", flags, log_handler, NULL);
    g_log_set_handler ("Gnome", flags, log_handler, NULL);
    g_log_set_default_handler (log_handler, NULL);
}

static void
cleanup_socket (gpointer unused)
{
        gkr_daemon_io_cleanup_socket_dir ();	
}

static gboolean
signal_handler (guint sig, gpointer unused)
{
	g_main_loop_quit (loop);
	return TRUE;
}

static int
sane_dup2 (int fd1, int fd2)
{
	int ret;

 retry:
	ret = dup2 (fd1, fd2);
	if (ret < 0 && errno == EINTR)
		goto retry;
	
	return ret;
}

static void
close_stdinout (void)
{
	int fd;
	
	fd = open ("/dev/null", O_RDONLY);
	sane_dup2 (fd, 0);
	close (fd);
	
	fd = open ("/dev/null", O_WRONLY);
	sane_dup2 (fd, 1);
	close (fd);

	fd = open ("/dev/null", O_WRONLY);
	sane_dup2 (fd, 2);
	close (fd);
}

static gboolean
lifetime_slave_pipe_io (GIOChannel  *channel,
			GIOCondition cond,
			gpointer     callback_data)
{
        gkr_daemon_io_cleanup_socket_dir ();
        _exit (2);
}

int
main (int argc, char *argv[])
{
	const char *path, *env;
	int fd;
	pid_t pid;
	gboolean foreground;
	gboolean daemon;
	GIOChannel *channel;
	GMainContext *ctx;
	int i;
	
	g_type_init ();
	g_thread_init (NULL);

	/* We do not use gcrypt in a multi-threaded manner */
	gcry_check_version (LIBGCRYPT_VERSION);
	
	if (!gkr_daemon_io_create_master_socket (&path))
		exit (1);
	
	gkr_cleanup_register (cleanup_socket, NULL); 
	
#ifdef HAVE_LOCALE_H
	/* internationalisation */
	setlocale (LC_ALL, "");
#endif

#ifdef HAVE_GETTEXT
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif


	srand (time (NULL));

	foreground = FALSE;
	daemon = FALSE;

	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			if (strcmp (argv[i], "-f") == 0)
				foreground = TRUE;
			if (strcmp (argv[i], "-d") == 0)
				daemon = TRUE;
		}
	}
	
	if (!foreground) {
		pid = fork ();
		if (pid == 0) {
			/* intermediated child */
			if (daemon) {
				pid = fork ();
				
				if (pid != 0) {
					/* still intermediated child */
					
					/* This process exits, so that the
					 * final child will inherit init as parent
					 * to avoid zombies
					 */
					if (pid == -1) {
						exit (1);
					} else {
						/* This is where we know the pid of the daemon.
						 * The initial process will waitpid until we exit,
						 * so there is no race */
						printf ("GNOME_KEYRING_SOCKET=%s\n", path);
						printf ("GNOME_KEYRING_PID=%d\n", (gint)pid);
						exit (0);
					}
				}
			}
			
			/* final child continues here */
		} else {
			if (daemon) {
				int status;
				/* Initial process, waits for intermediate child */
				if (pid == -1) {
					exit (1);
				}
				waitpid (pid, &status, 0);
				if (WEXITSTATUS (status) != 0) {
					exit (WEXITSTATUS (status));
				}
			} else {
				printf ("GNOME_KEYRING_SOCKET=%s\n", path);
				printf ("GNOME_KEYRING_PID=%d\n", (gint)pid);
			}
			
			exit (0);
		}
		
		close_stdinout ();

	} else {
		g_print ("GNOME_KEYRING_SOCKET=%s\n", path);
		g_print ("GNOME_KEYRING_PID=%d\n", (gint)getpid ());
	}

	/* Daemon process continues here */

        /* Send all warning or error messages to syslog, if a daemon */
        if (!foreground)
	        prepare_logging();

	loop = g_main_loop_new (NULL, FALSE);
	ctx = g_main_loop_get_context (loop);
	
	signal (SIGPIPE, SIG_IGN);
	gkr_unix_signal_connect (ctx, SIGINT, signal_handler, NULL);
	gkr_unix_signal_connect (ctx, SIGHUP, signal_handler, NULL);
	gkr_unix_signal_connect (ctx, SIGTERM, signal_handler, NULL);
             
	env = getenv ("GNOME_KEYRING_LIFETIME_FD");
	if (env && env[0]) {
		fd = atoi (env);
		if (fd != 0) {
			channel = g_io_channel_unix_new (fd);
			g_io_add_watch (channel,
					G_IO_IN | G_IO_HUP,
					lifetime_slave_pipe_io, NULL);
			g_io_channel_unref (channel);
		}
		
	}
	
	gkr_async_workers_init (loop);
	
#ifdef WITH_DBUS
	/* 
	 * We may be launched before the DBUS session, (ie: via PAM) 
	 * and DBus tries to launch itself somehow, so double check 
	 * that it has really started.
	 */ 
	env = getenv ("DBUS_SESSION_BUS_ADDRESS");
	if (env && env[0])
		gkr_daemon_dbus_setup (loop, path);
#endif

	g_main_loop_run (loop);

	/* Make sure no other threads are running */
	gkr_async_workers_stop_all ();
	
	/* This wraps everything up in order */
	gkr_cleanup_perform ();
	
	/* Final shutdown of anything workers running about */
	gkr_async_workers_uninit ();

	return 0;
}