/* search.c */

/* File search functionality */

/* fsv - 3D File System Visualizer */


#include "common.h"
#include "search.h"

#include <gtk/gtk.h>
#include <fnmatch.h>
#include <string.h>

#include "camera.h"
#include "colexp.h"
#include "dirtree.h"
#include "filelist.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"


/* Search widgets */
static GtkWidget *search_entry_w = NULL;
static GtkWidget *search_next_button_w = NULL;

/* Search results state */
static GList *search_results = NULL;
static int search_result_count = 0;
static int search_current_index = -1;


/* Case-insensitive substring search */
static boolean
str_contains_ci( const char *haystack, const char *needle )
{
	const char *h, *n;

	while (*haystack != '\0') {
		h = haystack;
		n = needle;
		while (*n != '\0' && *h != '\0') {
			if (g_ascii_tolower( *h ) != g_ascii_tolower( *n ))
				break;
			h++;
			n++;
		}
		if (*n == '\0')
			return TRUE;
		haystack++;
	}

	return FALSE;
}


/* Returns TRUE if pattern contains glob metacharacters */
static boolean
is_glob_pattern( const char *pattern )
{
	return (strchr( pattern, '*' ) != NULL ||
	        strchr( pattern, '?' ) != NULL ||
	        strchr( pattern, '[' ) != NULL);
}


/* Recursively search the filesystem tree for matching nodes */
static void
search_tree_recursive( GNode *node, const char *pattern, boolean use_glob, GList **results )
{
	GNode *child;
	const char *name;

	if (node == NULL)
		return;

	if (!NODE_IS_METANODE( node )) {
		name = NODE_DESC(node)->name;
		if (use_glob) {
			if (fnmatch( pattern, name, FNM_CASEFOLD ) == 0)
				G_LIST_APPEND(*results, node);
		}
		else {
			if (str_contains_ci( name, pattern ))
				G_LIST_APPEND(*results, node);
		}
	}

	/* Recurse into children */
	child = node->children;
	while (child != NULL) {
		search_tree_recursive( child, pattern, use_glob, results );
		child = child->next;
	}
}


/* Free previous search results */
static void
search_clear_results( void )
{
	if (search_results != NULL) {
		g_list_free( search_results );
		search_results = NULL;
	}
	search_result_count = 0;
	search_current_index = -1;
	if (search_next_button_w != NULL)
		gtk_widget_set_sensitive( search_next_button_w, FALSE );
}


/* Navigate to a search result by index */
static void
search_navigate_to_result( int index )
{
	GNode *node;
	GNode *parent_dnode;
	char strbuf[1024];

	if (search_results == NULL || index < 0 || index >= search_result_count)
		return;

	node = (GNode *)g_list_nth_data( search_results, index );
	if (node == NULL)
		return;

	search_current_index = index;

	/* Show match info in status bar */
	snprintf( strbuf, sizeof(strbuf), "Match %d of %d: %s",
	          index + 1, search_result_count, node_absname( node ) );
	window_statusbar( SB_LEFT, strbuf );

	/* Determine the parent directory */
	if (NODE_IS_DIR( node ))
		parent_dnode = node;
	else
		parent_dnode = node->parent;

	/* Expand ancestor directories so the node is visible */
	if (NODE_IS_DIR( parent_dnode ) && !NODE_IS_METANODE( parent_dnode ))
		colexp( parent_dnode, COLEXP_EXPAND_ANY );

	/* Show in directory tree and file list */
	filelist_show_entry( node );

	/* Highlight in 3D view */
	geometry_highlight_node( node, TRUE );

	/* Animate camera to the node */
	camera_look_at( node );
}


/* Callback: user pressed Enter in search entry */
static void
search_execute_cb( GtkWidget *widget, gpointer data )
{
	const char *pattern;

	if (globals.fsv_mode == FSV_SPLASH || globals.fsv_mode == FSV_NONE)
		return;

	pattern = gtk_entry_get_text( GTK_ENTRY(search_entry_w) );
	if (pattern == NULL || strlen( pattern ) == 0)
		return;

	/* Clear old results */
	search_clear_results( );

	/* Search the filesystem tree */
	search_tree_recursive( globals.fstree, pattern, is_glob_pattern( pattern ), &search_results );
	search_result_count = g_list_length( search_results );

	if (search_result_count == 0) {
		window_statusbar( SB_LEFT, _("No matches found") );
		return;
	}

	/* Enable Next button if multiple results */
	if (search_result_count > 1)
		gtk_widget_set_sensitive( search_next_button_w, TRUE );

	/* Navigate to first result */
	search_navigate_to_result( 0 );
}


/* Callback: user clicked Next button */
static void
search_next_cb( GtkWidget *widget, gpointer data )
{
	if (search_result_count <= 0)
		return;

	/* Cycle to next result (wrap around) */
	search_navigate_to_result( (search_current_index + 1) % search_result_count );
}


/* Receives search widgets from window_init( ) */
void
search_pass_widgets( GtkWidget *entry_w, GtkWidget *next_button_w )
{
	search_entry_w = entry_w;
	search_next_button_w = next_button_w;

	g_signal_connect( G_OBJECT(search_entry_w), "activate",
	                    G_CALLBACK(search_execute_cb), NULL );
	g_signal_connect( G_OBJECT(search_next_button_w), "clicked",
	                    G_CALLBACK(search_next_cb), NULL );
}


/* end search.c */
