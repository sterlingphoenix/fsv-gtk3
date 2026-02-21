/* fsv.c */

/* Program entry */

/* fsv - 3D File System Visualizer
 *
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Updates (c) 2026 sterlingphoenix <fsv@freakzilla.com>
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "common.h"
#include "fsv.h"

#include <gtk/gtk.h>
#include <unistd.h> /* symlink( ) */
#include "getopt.h"

#include "about.h"
#include "animation.h"
#include "camera.h"
#include "color.h" /* color_init( ), color_write_config( ) */
#include "filelist.h"
#include "geometry.h"
#include "gui.h" /* gui_update( ) */
#include "nvstore.h"
#include "ogl.h" /* ogl_gl_query( ) */
#include "scanfs.h"
#include "window.h"


/* Mapping of CSS cursor names to traditional X cursor names.
 * Incomplete cursor themes (e.g. whiteglass) may have the traditional
 * X cursors but lack the CSS names that GTK 3 widgets request.
 * Multiple fallbacks are listed per CSS name (first match wins). */
#define MAX_CURSOR_FALLBACKS 3
static const struct {
	const char *css;
	const char *traditional[MAX_CURSOR_FALLBACKS];
} cursor_aliases[] = {
	{ "col-resize",   { "sb_h_double_arrow" } },
	{ "row-resize",   { "sb_v_double_arrow" } },
	{ "not-allowed",  { "crossed_circle", "X_cursor", "pirate" } },
	{ "move",         { "fleur" } },
	{ "wait",         { "watch" } },
	{ "ns-resize",    { "sb_v_double_arrow" } },
	{ "ew-resize",    { "sb_h_double_arrow" } },
	{ "pointer",      { "hand2", "hand" } },
	{ "progress",     { "left_ptr_watch" } },
	{ "text",         { "xterm" } },
	{ "crosshair",    { "cross", "tcross" } },
	{ "all-scroll",   { "fleur" } },
};


/* Detect cursor theme name from environment/config (before gtk_init) */
static char *
detect_cursor_theme( void )
{
	const char *env;
	char *theme = NULL;
	char *path;
	GKeyFile *kf;

	/* XCURSOR_THEME takes precedence */
	env = g_getenv( "XCURSOR_THEME" );
	if (env != NULL && env[0] != '\0')
		return g_strdup( env );

	/* GSettings (GNOME desktop) */
	{
		GSettingsSchemaSource *source;
		GSettingsSchema *schema;

		source = g_settings_schema_source_get_default( );
		if (source != NULL) {
			schema = g_settings_schema_source_lookup( source,
				"org.gnome.desktop.interface", TRUE );
			if (schema != NULL) {
				GSettings *settings = g_settings_new(
					"org.gnome.desktop.interface" );
				theme = g_settings_get_string( settings,
					"cursor-theme" );
				g_object_unref( settings );
				g_settings_schema_unref( schema );
				if (theme != NULL && theme[0] != '\0')
					return theme;
				g_free( theme );
				theme = NULL;
			}
		}
	}

	/* User's GTK 3 settings file */
	kf = g_key_file_new( );
	path = g_build_filename( g_get_home_dir( ), ".config", "gtk-3.0",
		"settings.ini", NULL );
	if (g_key_file_load_from_file( kf, path, G_KEY_FILE_NONE, NULL ))
		theme = g_key_file_get_string( kf, "Settings",
			"gtk-cursor-theme-name", NULL );
	g_key_file_free( kf );
	g_free( path );
	if (theme != NULL)
		return theme;

	/* System-wide GTK 3 settings */
	kf = g_key_file_new( );
	if (g_key_file_load_from_file( kf, "/etc/gtk-3.0/settings.ini",
			G_KEY_FILE_NONE, NULL ))
		theme = g_key_file_get_string( kf, "Settings",
			"gtk-cursor-theme-name", NULL );
	g_key_file_free( kf );
	if (theme != NULL)
		return theme;

	/* XDG default cursor theme */
	kf = g_key_file_new( );
	path = g_build_filename( g_get_home_dir( ), ".icons", "default",
		"index.theme", NULL );
	if (g_key_file_load_from_file( kf, path, G_KEY_FILE_NONE, NULL ))
		theme = g_key_file_get_string( kf, "Icon Theme",
			"Inherits", NULL );
	g_key_file_free( kf );
	g_free( path );
	if (theme != NULL)
		return theme;

	kf = g_key_file_new( );
	if (g_key_file_load_from_file( kf,
			"/usr/share/icons/default/index.theme",
			G_KEY_FILE_NONE, NULL ))
		theme = g_key_file_get_string( kf, "Icon Theme",
			"Inherits", NULL );
	g_key_file_free( kf );
	if (theme != NULL)
		return theme;

	return g_strdup( "default" );
}


/* Find a cursor theme's cursor directory in standard search paths.
 * Returns absolute path or NULL. Caller must g_free( ) the result. */
static char *
find_cursor_dir( const char *theme_name )
{
	const char *env;
	char *path;
	char *search_dirs[4];
	int i;

	/* Check XCURSOR_PATH first */
	env = g_getenv( "XCURSOR_PATH" );
	if (env != NULL) {
		char **parts = g_strsplit( env, ":", -1 );
		for (i = 0; parts[i] != NULL; i++) {
			path = g_build_filename( parts[i], theme_name,
				"cursors", NULL );
			if (g_file_test( path, G_FILE_TEST_IS_DIR )) {
				g_strfreev( parts );
				return path;
			}
			g_free( path );
		}
		g_strfreev( parts );
	}

	/* Search standard XDG/legacy locations */
	search_dirs[0] = g_build_filename( g_get_user_data_dir( ),
		"icons", NULL );
	search_dirs[1] = g_build_filename( g_get_home_dir( ),
		".icons", NULL );
	search_dirs[2] = g_strdup( "/usr/share/icons" );
	search_dirs[3] = NULL;

	for (i = 0; search_dirs[i] != NULL; i++) {
		path = g_build_filename( search_dirs[i], theme_name,
			"cursors", NULL );
		if (g_file_test( path, G_FILE_TEST_IS_DIR )) {
			for (int j = 0; search_dirs[j] != NULL; j++)
				g_free( search_dirs[j] );
			return path;
		}
		g_free( path );
	}

	for (i = 0; search_dirs[i] != NULL; i++)
		g_free( search_dirs[i] );
	return NULL;
}


/* Creates a temporary overlay directory with symlinks from CSS cursor
 * names to traditional X cursor names for the active cursor theme.
 * This allows GTK 3 widgets (GtkPaned dividers, GtkTreeView column
 * resize handles, etc.) to show correct cursors even when the theme
 * lacks CSS-named cursor files.
 * Must be called BEFORE gtk_init( ) so that Xcursor picks up the
 * modified XCURSOR_PATH when the display connection is opened. */
static void
cursor_theme_fixup( void )
{
	char *theme_name;
	char *cursor_dir;
	char *tmpdir;
	char *overlay;
	gboolean created_any = FALSE;
	unsigned int i;

	theme_name = detect_cursor_theme( );
	cursor_dir = find_cursor_dir( theme_name );
	if (cursor_dir == NULL) {
		g_free( theme_name );
		return;
	}

	/* Create temporary overlay directory */
	tmpdir = g_dir_make_tmp( "fsv-cursors-XXXXXX", NULL );
	if (tmpdir == NULL) {
		g_free( theme_name );
		g_free( cursor_dir );
		return;
	}
	overlay = g_build_filename( tmpdir, theme_name, "cursors", NULL );
	g_mkdir_with_parents( overlay, 0700 );

	/* For each CSS name that the theme lacks, create a symlink
	 * to the first matching traditional X cursor */
	for (i = 0; i < G_N_ELEMENTS( cursor_aliases ); i++) {
		char *css_path;
		int j;

		css_path = g_build_filename( cursor_dir,
			cursor_aliases[i].css, NULL );

		if (!g_file_test( css_path, G_FILE_TEST_EXISTS )) {
			for (j = 0; j < MAX_CURSOR_FALLBACKS; j++) {
				char *x_path, *link_path;

				if (cursor_aliases[i].traditional[j] == NULL)
					break;
				x_path = g_build_filename( cursor_dir,
					cursor_aliases[i].traditional[j],
					NULL );
				if (g_file_test( x_path, G_FILE_TEST_EXISTS )) {
					link_path = g_build_filename( overlay,
						cursor_aliases[i].css, NULL );
					if (symlink( x_path, link_path ) == 0)
						created_any = TRUE;
					g_free( link_path );
					g_free( x_path );
					break;
				}
				g_free( x_path );
			}
		}

		g_free( css_path );
	}

	if (created_any) {
		/* Prepend overlay to XCURSOR_PATH.
		 * If XCURSOR_PATH was already set, preserve it.
		 * Otherwise, include the standard search paths so
		 * Xcursor can still find the original theme. */
		const char *old_path = g_getenv( "XCURSOR_PATH" );
		char *new_path;

		if (old_path != NULL) {
			new_path = g_strdup_printf( "%s:%s",
				tmpdir, old_path );
		}
		else {
			const char *data_home = g_get_user_data_dir( );
			const char *home = g_get_home_dir( );
			new_path = g_strdup_printf(
				"%s:%s/icons:%s/.icons"
				":/usr/share/icons:/usr/share/pixmaps",
				tmpdir, data_home, home );
		}
		g_setenv( "XCURSOR_PATH", new_path, TRUE );
		g_free( new_path );
	}
	else {
		/* No symlinks needed; clean up empty overlay */
		rmdir( overlay );
		char *theme_dir = g_build_filename( tmpdir,
			theme_name, NULL );
		rmdir( theme_dir );
		g_free( theme_dir );
		rmdir( tmpdir );
	}

	g_free( overlay );
	g_free( tmpdir );
	g_free( cursor_dir );
	g_free( theme_name );
}


/* Suppresses "Unable to load X from the cursor theme" GDK messages.
 * These are benign warnings that may still occur if the cursor theme
 * lacks both the CSS name and its traditional X equivalent. */
static void
gdk_message_handler( const gchar *log_domain, GLogLevelFlags log_level,
                     const gchar *message, G_GNUC_UNUSED gpointer user_data )
{
	if (message != NULL && strstr( message, "from the cursor theme" ) != NULL)
		return;
	g_log_default_handler( log_domain, log_level, message, user_data );
}


/* Identifiers for command-line options */
enum {
	OPT_DISCV,
	OPT_MAPV,
	OPT_TREEV,
	OPT_CACHEDIR,
	OPT_NOCACHE,
	OPT_HELP
};


/* Initial visualization mode */
static FsvMode initial_fsv_mode = FSV_MAPV;

/* Token strings for config file */
static const char *tokens_fsv_mode[] = { "discv", "mapv", "treev", NULL };

/* Command-line options */
static struct option cli_opts[] = {
	{ "discv", no_argument, NULL, OPT_DISCV },
	{ "mapv", no_argument, NULL, OPT_MAPV },
	{ "treev", no_argument, NULL, OPT_TREEV },
	{ "cachedir", required_argument, NULL, OPT_CACHEDIR },
	{ "nocache", no_argument, NULL, OPT_NOCACHE },
	{ "help", no_argument, NULL, OPT_HELP },
	{ NULL, 0, NULL, 0 }
};

/* Usage summary */
static const char usage_summary[] = __("\n"
    "fsv - 3D File System Visualizer\n"
    "      Version " VERSION "\n"
    "\n"
    "Usage: %s [rootdir] [options]\n"
    "  rootdir      Root directory for visualization\n"
    "               (defaults to current directory)\n"
    "  --mapv       Start in Map Visualisation mode (default)\n"
    "  --discv      Start in Disc Visualisation mode\n"
    "  --treev      Start in Tree Visualisation mode\n"
    "  --help       Print this help and exit\n"
    "\n");


/* Helper function for fsv_set_mode( ) */
static void
initial_camera_pan( void *data )
{
	char *mesg = (char *)data;
	/* To prevent root_dnode from appearing twice in a row at
	 * the bottom of the node history stack */
	G_LIST_PREPEND(globals.history, NULL);

	if (!strcmp( mesg, "new_fs" )) {
		/* First look at new filesystem */
		camera_look_at_full( root_dnode, MORPH_SIGMOID, 2.0 );
	}
	else {
		/* Same filesystem, different visualization mode */
		if (globals.fsv_mode == FSV_TREEV) {
			/* Enter TreeV mode with an L-shaped pan */
			camera_treev_lpan_look_at( globals.current_node, 1.0 );
		}
		else
			camera_look_at_full( globals.current_node, MORPH_INV_QUADRATIC, 1.0 );
	}
}


/* Switches between visualization modes */
void
fsv_set_mode( FsvMode mode )
{
	boolean first_init = FALSE;

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		/* Queue desired mode */
		initial_fsv_mode = mode;
		return;

		case FSV_NONE:
		/* Filesystem's first appearance */
		first_init = TRUE;
		break;

		default:
		/* Set initial mode for next time */
		initial_fsv_mode = mode;
		break;
	}

	/* Generate appropriate visualization geometry */
	geometry_init( mode );

	/* Set up initial camera state */
	camera_init( mode, first_init );

	globals.fsv_mode = mode;

	/* Ensure that About presentation is not up */
	about( ABOUT_END );

	/* Render one frame before performing the initial camera pan.
	 * There are two separate reasons for doing this: */
	if (first_init) {
		/* 1. Practical limitations make the first frame take an
		 * unusually long time to render, so this avoids a really
		 * unpleasant camera jump */
		schedule_event( initial_camera_pan, "new_fs", 1 );
	}
	else {
		/* 2. In order to do a camera pan, the geometry needs to
		 * be defined. We just called geometry_init( ), but if the
		 * camera's going to a non-root node, it may very well not
		 * have been laid out yet (but it will be when drawn) */
		schedule_event( initial_camera_pan, "", 1 );
	}
}


/* Performs filesystem scan and first-time initialization */
void
fsv_load( const char *dir )
{
	/* Lock down interface */
	window_set_access( FALSE );

	/* Bring up splash screen */
	globals.fsv_mode = FSV_SPLASH;
	redraw( );

	/* Reset scrollbars (disable scrolling) */
	camera_update_scrollbars( TRUE );

	gui_update( );

	/* Scan filesystem */
	scanfs( dir );

	/* Clear/reset node history */
	g_list_free( globals.history );
	globals.history = NULL;
	globals.current_node = root_dnode;

	/* Initialize file list */
	filelist_init( );
	gui_update( );

	/* Initialize visualization */
	globals.fsv_mode = FSV_NONE;
	fsv_set_mode( initial_fsv_mode );
}


void
fsv_write_config( void )
{
	NVStore *fsvrc;

	fsvrc = nvs_open( CONFIG_FILE );
	nvs_write_int_token( fsvrc, "/fsv/mode", globals.fsv_mode, tokens_fsv_mode );
	nvs_close( fsvrc );

	color_write_config( );
}


int
main( int argc, char **argv )
{
	int opt_id;
	char *root_dir;

	/* Initialize global variables */
	globals.fstree = NULL;
	globals.history = NULL;
	/* Set sane camera state so setup_modelview_matrix( ) in ogl.c
	 * doesn't choke. (It does get called in splash screen mode) */
	camera->fov = 45.0;
	camera->near_clip = 1.0;
	camera->far_clip = 2.0;

#ifdef DEBUG
	debug_init( );
#endif
#ifdef ENABLE_NLS
	/* Initialize internationalization (i8e i18n :-) */
	setlocale( LC_ALL, "" );
	bindtextdomain( PACKAGE, LOCALEDIR );
	textdomain( PACKAGE );
#endif

	/* Read saved visualization mode from config (CLI options override) */
	{
		NVStore *fsvrc = nvs_open( CONFIG_FILE );
		int mode = nvs_read_int_token_default( fsvrc, "/fsv/mode", tokens_fsv_mode, FSV_MAPV );
		initial_fsv_mode = (FsvMode)mode;
		nvs_close( fsvrc );
	}

	/* Parse command-line options */
	for (;;) {
		opt_id = getopt_long( argc, argv, "", cli_opts, NULL );
		if (opt_id < 0)
			break;
		switch (opt_id) {
			case OPT_DISCV:
			/* --discv */
			initial_fsv_mode = FSV_DISCV;
			break;

			case OPT_MAPV:
			/* --mapv */
			initial_fsv_mode = FSV_MAPV;
			break;

			case OPT_TREEV:
			/* --treev */
			initial_fsv_mode = FSV_TREEV;
			break;

			case OPT_CACHEDIR:
			/* --cachedir <dir> */
			printf( "cache directory: %s\n", optarg );
			printf( "(caching not yet implemented)\n" );
			/* TODO: Implement caching */
			break;

			case OPT_NOCACHE:
			/* --nocache */
			/* TODO: Implement caching */
			break;

			case OPT_HELP:
			/* --help */
			default:
			/* unrecognized option */
			printf( _(usage_summary), argv[0] );
			fflush( stdout );
			exit( EXIT_SUCCESS );
			break;
		}
	}

	/* Determine root directory */
	if (optind < argc) {
                /* From command line */
		root_dir = xstrdup( argv[optind++] );
		if (optind < argc) {
			/* Excess arguments! */
			fprintf( stderr, _("Junk in command line:") );
			while (optind < argc)
				fprintf( stderr, " %s", argv[optind++] );
			fprintf( stderr, "\n" );
			fflush( stderr );
		}
	}
	else {
		/* Use current directory */
		root_dir = xstrdup( "." );
	}

	/* Request a legacy (compatibility profile) GL context.
	 * GtkGLArea defaults to core profile, which doesn't support
	 * the legacy GL calls (glBegin/glEnd, display lists, fixed-
	 * function lighting) used throughout this codebase. */
	{
		const char *gdk_gl = g_getenv( "GDK_GL" );
		if (gdk_gl == NULL) {
			g_setenv( "GDK_GL", "legacy", TRUE );
		}
		else if (strstr( gdk_gl, "legacy" ) == NULL) {
			char *new_val = g_strdup_printf( "%s,legacy", gdk_gl );
			g_setenv( "GDK_GL", new_val, TRUE );
			g_free( new_val );
		}
	}

	/* Patch incomplete cursor themes before GTK opens the display */
	cursor_theme_fixup( );

	/* Initialize GTK+ */
	gtk_init( &argc, &argv );

	/* Suppress any remaining cursor theme warnings from GDK */
	g_log_set_handler( "Gdk", G_LOG_LEVEL_MESSAGE,
		gdk_message_handler, NULL );

	/* Check for OpenGL support */
	if (!ogl_gl_query( ))
		quit( _("fsv requires OpenGL support.") );

	window_init( initial_fsv_mode );
	color_init( );

	fsv_load( root_dir );
	xfree( root_dir );

	gtk_main( );

	return 0;
}


/* end fsv.c */
