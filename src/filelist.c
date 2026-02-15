/* filelist.c */

/* File list control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * This program is free software; you can redistribute it and/or
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
#include "filelist.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "dialog.h"
#include "dirtree.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"


/* Time for the filelist to scroll to a given entry (in seconds) */
#define FILELIST_SCROLL_TIME 0.5

/* Model column indices for the normal file list (1 visible column).
 * Model: pixbuf (0), name (1), node_ptr (2) */
enum {
	FLIST_COL_PIXBUF = 0,
	FLIST_COL_NAME   = 1,
	FLIST_COL_DATA   = 2
};

/* Model column indices for the scan monitor (3 visible columns).
 * Model: pixbuf (0), type (1), found (2), bytes (3), data (4) */
enum {
	SCANMON_COL_PIXBUF = 0,
	SCANMON_COL_TYPE   = 1,
	SCANMON_COL_FOUND  = 2,
	SCANMON_COL_BYTES  = 3,
	SCANMON_COL_DATA   = 4
};


/* The file list widget (GtkTreeView) */
static GtkWidget *file_tree_w;

/* Directory currently listed */
static GNode *filelist_current_dnode;

/* Mini node type icons (GdkPixbuf) */
static Icon node_type_mini_icons[NUM_NODE_TYPES];


/* Loads the mini node type icons (from XPM data) */
static void
filelist_icons_init( void )
{
	int i;

	for (i = 1; i < NUM_NODE_TYPES; i++)
		node_type_mini_icons[i].pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)node_type_mini_xpms[i] );
}


/* Correspondence from window_init( ) */
void
filelist_pass_widget( GtkWidget *tree_w )
{
	file_tree_w = tree_w;
	filelist_icons_init( );
}


/* This makes entries in the file list selectable or unselectable,
 * depending on whether the directory they are in is expanded or not */
void
filelist_reset_access( void )
{
	boolean enabled;

	enabled = dirtree_entry_expanded( filelist_current_dnode );
	gtk_widget_set_sensitive( file_tree_w, enabled );

	/* Extra fluff for interface niceness */
	if (enabled)
		gui_cursor( file_tree_w, -1 );
	else {
		GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(file_tree_w) );
		gtk_tree_selection_unselect_all( sel );
		gui_cursor( file_tree_w, GDK_X_CURSOR );
	}
}


/* Compare function for sorting nodes alphabetically */
static int
compare_node( GNode *a, GNode *b )
{
	return strcmp( NODE_DESC(a)->name, NODE_DESC(b)->name );
}


/* Helper: find a row by its data pointer and return its path.
 * Returns NULL if not found. Caller must free the path. */
static GtkTreePath *
find_path_by_data( GtkTreeView *tree_view, gpointer data )
{
	GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
	GtkTreeIter iter;
	gboolean valid;
	int num_cols_val;
	int data_col;

	/* Get data column index */
	num_cols_val = *(int *)g_object_get_data( G_OBJECT(tree_view), "num_cols" );
	data_col = num_cols_val + 1;

	valid = gtk_tree_model_get_iter_first( model, &iter );
	while (valid) {
		gpointer row_data;
		gtk_tree_model_get( model, &iter, data_col, &row_data, -1 );
		if (row_data == data)
			return gtk_tree_model_get_path( model, &iter );
		valid = gtk_tree_model_iter_next( model, &iter );
	}
	return NULL;
}


/* Displays contents of a directory in the file list */
void
filelist_populate( GNode *dnode )
{
	GNode *node;
	GList *node_list = NULL, *node_llink;
	Icon *icon;
	GtkListStore *store;
	GtkTreeIter iter;
	int count = 0;
	char strbuf[64];

	g_assert( NODE_IS_DIR(dnode) );

	/* Get an alphabetized list of directory's immediate children */
	node = dnode->children;
	while (node != NULL) {
		G_LIST_PREPEND(node_list, node);
		node = node->next;
	}
	G_LIST_SORT(node_list, compare_node);

	/* Update file list */
	store = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(file_tree_w) ));
	gtk_list_store_clear( store );
	node_llink = node_list;
	while (node_llink != NULL) {
		node = (GNode *)node_llink->data;
		icon = &node_type_mini_icons[NODE_DESC(node)->type];

		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter,
			FLIST_COL_PIXBUF, icon->pixbuf,
			FLIST_COL_NAME, NODE_DESC(node)->name,
			FLIST_COL_DATA, node,
			-1 );

		++count;
		node_llink = node_llink->next;
	}

	g_list_free( node_list );

	/* Set node count message in the left statusbar */
	switch (count) {
		case 0:
		strcpy( strbuf, "" );
		break;

		case 1:
		strcpy( strbuf, _("1 node") );
		break;

		default:
		sprintf( strbuf, _("%d nodes"), count );
		break;
	}
	window_statusbar( SB_LEFT, strbuf );

	filelist_current_dnode = dnode;
	filelist_reset_access( );
}


/* This updates the file list to show (and select) a particular node
 * entry. The directory tree is also updated appropriately */
void
filelist_show_entry( GNode *node )
{
	GNode *dnode;
	GtkTreePath *path;
	GtkTreeSelection *sel;

	/* Corresponding directory */
	if (NODE_IS_DIR(node))
		dnode = node;
	else
		dnode = node->parent;

	if (dnode != filelist_current_dnode) {
		/* Scroll directory tree to proper entry */
		dirtree_entry_show( dnode );
	}

	/* Scroll file list to proper entry */
	path = find_path_by_data( GTK_TREE_VIEW(file_tree_w), node );
	sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(file_tree_w) );
	if (path != NULL) {
		gtk_tree_selection_select_path( sel, path );
		gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(file_tree_w), path, NULL, TRUE, 0.5, 0.0 );
		gtk_tree_path_free( path );
	}
	else {
		gtk_tree_selection_unselect_all( sel );
	}
}


/* Callback for a click in the file list area */
static int
filelist_select_cb( GtkWidget *tree_w, GdkEventButton *ev_button )
{
	GNode *node;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	int data_col;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(tree_w),
			(int)ev_button->x, (int)ev_button->y,
			&path, &column, NULL, NULL ))
		return FALSE;

	model = gtk_tree_view_get_model( GTK_TREE_VIEW(tree_w) );
	if (!gtk_tree_model_get_iter( model, &iter, path )) {
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* Get node pointer from data column */
	data_col = *(int *)g_object_get_data( G_OBJECT(tree_w), "num_cols" ) + 1;
	gtk_tree_model_get( model, &iter, data_col, &node, -1 );
	if (node == NULL) {
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* A single-click from button 1 highlights the node and shows the name */
	if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS)) {
		geometry_highlight_node( node, FALSE );
		window_statusbar( SB_RIGHT, node_absname( node ) );
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* A double-click from button 1 gets the camera moving */
	if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS)) {
		camera_look_at( node );
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* A click from button 3 selects the row, highlights the node,
	 * shows the name, and pops up a context-sensitive menu */
	if (ev_button->button == 3) {
		sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tree_w) );
		gtk_tree_selection_select_path( sel, path );
		geometry_highlight_node( node, FALSE );
		window_statusbar( SB_RIGHT, node_absname( node ) );
		context_menu( node, ev_button );
		gtk_tree_path_free( path );
		return FALSE;
	}

	gtk_tree_path_free( path );
	return FALSE;
}


/* Creates/initializes the file list widget */
void
filelist_init( void )
{
	GtkWidget *parent_w;

	/* Replace current tree view widget with a single-column one */
	parent_w = gtk_widget_get_parent( gtk_widget_get_parent( file_tree_w ) );
	gtk_widget_destroy( gtk_widget_get_parent( file_tree_w ) );
	file_tree_w = gui_clist_add( parent_w, 1, NULL );
	g_signal_connect( G_OBJECT(file_tree_w), "button_press_event", G_CALLBACK(filelist_select_cb), NULL );

	filelist_populate( root_dnode );

	/* Do this so that directory tree gets scrolled to the top at
	 * end of initial camera pan (right after filesystem scan) */
	filelist_current_dnode = NULL;
}


/* This replaces the file list widget with another one made specifically
 * to monitor the progress of an impending scan */
void
filelist_scan_monitor_init( void )
{
	char *col_titles[3];
	GtkWidget *parent_w;
	GtkListStore *store;
	GtkTreeIter iter;
	Icon *icon;
	int i;

	col_titles[0] = _("Type");
	col_titles[1] = _("Found");
	col_titles[2] = _("Bytes");

	/* Replace current tree view widget with a 3-column one */
	parent_w = gtk_widget_get_parent( gtk_widget_get_parent( file_tree_w ) );
	gtk_widget_destroy( gtk_widget_get_parent( file_tree_w ) );
	file_tree_w = gui_clist_add( parent_w, 3, col_titles );

	/* Place icons and static text */
	store = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(file_tree_w) ));
	for (i = 1; i <= NUM_NODE_TYPES; i++) {
		gtk_list_store_append( store, &iter );
		if (i < NUM_NODE_TYPES) {
			icon = &node_type_mini_icons[i];
			gtk_list_store_set( store, &iter,
				SCANMON_COL_PIXBUF, icon->pixbuf,
				SCANMON_COL_TYPE, _(node_type_plural_names[i]),
				-1 );
		}
		else {
			gtk_list_store_set( store, &iter,
				SCANMON_COL_TYPE, _("TOTAL"),
				-1 );
		}
	}
}


/* Updates the scan-monitoring file list with the given values */
void
filelist_scan_monitor( int *node_counts, int64 *size_counts )
{
	GtkListStore *store;
	GtkTreeIter iter;
	const char *str;
	int64 size_total = 0;
	int node_total = 0;
	int i;
	gboolean valid;

	store = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(file_tree_w) ));
	valid = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter );

	for (i = 1; i <= NUM_NODE_TYPES && valid; i++) {
		/* Column 2: Found count */
		if (i < NUM_NODE_TYPES) {
			str = i64toa( node_counts[i] );
			node_total += node_counts[i];
		}
		else
			str = i64toa( node_total );
		gtk_list_store_set( store, &iter, SCANMON_COL_FOUND, str, -1 );

		/* Column 3: Bytes */
		if (i < NUM_NODE_TYPES) {
			str = i64toa( size_counts[i] );
			size_total += size_counts[i];
		}
		else
			str = i64toa( size_total );
		gtk_list_store_set( store, &iter, SCANMON_COL_BYTES, str, -1 );

		valid = gtk_tree_model_iter_next( GTK_TREE_MODEL(store), &iter );
	}
}


/* Creates the tree view widget used in the "Contents" page of the Properties
 * dialog for a directory */
GtkWidget *
dir_contents_list( GNode *dnode )
{
	char *col_titles[2];
	GtkListStore *store;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	Icon *icon;
	int i;

	g_assert( NODE_IS_DIR(dnode) );

	col_titles[0] = _("Node type");
	col_titles[1] = _("Quantity");

	/* Create a simple 2-column list (not inside a scrolled window) */
	store = gtk_list_store_new( 3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING );
	tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) ));
	g_object_unref( store );

	/* Column 0: icon + type name */
	column = gtk_tree_view_column_new( );
	gtk_tree_view_column_set_title( column, col_titles[0] );
	renderer = gtk_cell_renderer_pixbuf_new( );
	gtk_tree_view_column_pack_start( column, renderer, FALSE );
	gtk_tree_view_column_add_attribute( column, renderer, "pixbuf", 0 );
	renderer = gtk_cell_renderer_text_new( );
	gtk_tree_view_column_pack_start( column, renderer, TRUE );
	gtk_tree_view_column_add_attribute( column, renderer, "text", 1 );
	gtk_tree_view_append_column( tree_view, column );

	/* Column 1: quantity */
	renderer = gtk_cell_renderer_text_new( );
	column = gtk_tree_view_column_new_with_attributes( col_titles[1], renderer, "text", 2, NULL );
	gtk_tree_view_append_column( tree_view, column );

	/* Selection mode */
	gtk_tree_selection_set_mode( gtk_tree_view_get_selection( tree_view ), GTK_SELECTION_SINGLE );

	/* Populate */
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		gtk_list_store_append( store, &iter );
		icon = &node_type_mini_icons[i];
		gtk_list_store_set( store, &iter,
			0, icon->pixbuf,
			1, _(node_type_plural_names[i]),
			2, (char *)i64toa( DIR_NODE_DESC(dnode)->subtree.counts[i] ),
			-1 );
	}

	return GTK_WIDGET(tree_view);
}


/* end filelist.c */
