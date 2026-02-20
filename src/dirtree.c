/* dirtree.c */

/* Directory tree control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Updates (c) 2026 sterlingphoenix <fsv@freakzilla.com>
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
#include "dirtree.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "colexp.h"
#include "dialog.h"
#include "filelist.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"

/* Mini collapsed/expanded directory icon XPM's */
#define mini_folder_xpm mini_folder_closed_xpm
#include "xmaps/mini-folder.xpm"
#include "xmaps/mini-folder-open.xpm"


/* Time for the directory tree to scroll to a given entry (in seconds) */
#define DIRTREE_SCROLL_TIME 0.5

/* Column indices in the TreeStore (must match gui.c CTREE_COL_* enum) */
enum {
	COL_PIXBUF = 0,
	COL_NAME   = 1,
	COL_DATA   = 2
};


/* The directory tree widget (GtkTreeView) */
static GtkWidget *dir_tree_w;

/* Mini collapsed/expanded directory icons (GdkPixbuf) */
static Icon dir_colexp_mini_icons[2];

/* Current directory */
static GNode *dirtree_current_dnode;


/* Helper: get the TreeStore model from the tree view */
static GtkTreeStore *
get_tree_store( void )
{
	return GTK_TREE_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(dir_tree_w) ));
}


/* Helper: get GNode from a tree path */
static GNode *
dnode_from_path( GtkTreePath *path )
{
	GtkTreeStore *store = get_tree_store( );
	GtkTreeIter iter;
	GNode *dnode = NULL;

	if (gtk_tree_model_get_iter( GTK_TREE_MODEL(store), &iter, path ))
		gtk_tree_model_get( GTK_TREE_MODEL(store), &iter, COL_DATA, &dnode, -1 );

	return dnode;
}


/* Helper: get GNode from a tree iter */
static GNode *
dnode_from_iter( GtkTreeIter *iter )
{
	GtkTreeStore *store = get_tree_store( );
	GNode *dnode = NULL;

	gtk_tree_model_get( GTK_TREE_MODEL(store), iter, COL_DATA, &dnode, -1 );
	return dnode;
}


/* Callback for button press in the directory tree area */
static int
dirtree_select_cb( GtkWidget *tree_w, GdkEventButton *ev_button )
{
	GNode *dnode;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	/* Find which row was clicked */
	if (!gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(tree_w),
			(int)ev_button->x, (int)ev_button->y,
			&path, &column, NULL, NULL ))
		return FALSE;

	dnode = dnode_from_path( path );
	if (dnode == NULL) {
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* A single-click from button 1 highlights the node, shows the
	 * name, and updates the file list if necessary */
	if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS)) {
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		gtk_tree_path_free( path );
		return FALSE;
	}

	/* A double-click from button 1 gets the camera moving */
	if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS)) {
		camera_look_at( dnode );
		/* Preempt the forthcoming tree expand/collapse */
		g_signal_stop_emission_by_name( G_OBJECT(tree_w), "button_press_event" );
		gtk_tree_path_free( path );
		return TRUE;
	}

	/* A click from button 3 selects the row, highlights the node,
	 * shows the name, updates the file list if necessary, and brings
	 * up a context-sensitive menu */
	if (ev_button->button == 3) {
		sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tree_w) );
		gtk_tree_selection_select_path( sel, path );
		geometry_highlight_node( dnode, FALSE );
		window_statusbar( SB_RIGHT, node_absname( dnode ) );
		if (dnode != dirtree_current_dnode)
			filelist_populate( dnode );
		dirtree_current_dnode = dnode;
		context_menu( dnode, ev_button );
		gtk_tree_path_free( path );
		return FALSE;
	}

	gtk_tree_path_free( path );
	return FALSE;
}


/* Callback for collapse of a directory tree entry */
static void
dirtree_collapse_cb( G_GNUC_UNUSED GtkTreeView *tree_view, GtkTreeIter *iter, G_GNUC_UNUSED GtkTreePath *path, G_GNUC_UNUSED gpointer user_data )
{
	GtkTreeStore *store;
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	dnode = dnode_from_iter( iter );
	if (dnode == NULL)
		return;

	/* Update the icon to collapsed */
	store = get_tree_store( );
	gtk_tree_store_set( store, iter, COL_PIXBUF, dir_colexp_mini_icons[0].pixbuf, -1 );

	colexp( dnode, COLEXP_COLLAPSE_RECURSIVE );
}


/* Callback for expand of a directory tree entry */
static void
dirtree_expand_cb( G_GNUC_UNUSED GtkTreeView *tree_view, GtkTreeIter *iter, G_GNUC_UNUSED GtkTreePath *path, G_GNUC_UNUSED gpointer user_data )
{
	GtkTreeStore *store;
	GNode *dnode;

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	dnode = dnode_from_iter( iter );
	if (dnode == NULL)
		return;

	/* Update the icon to expanded */
	store = get_tree_store( );
	gtk_tree_store_set( store, iter, COL_PIXBUF, dir_colexp_mini_icons[1].pixbuf, -1 );

	colexp( dnode, COLEXP_EXPAND );
}


/* Loads the mini collapsed/expanded directory icons (from XPM data) */
static void
dirtree_icons_init( void )
{
	static char **dir_colexp_mini_xpms[] = {
		mini_folder_closed_xpm,
		mini_folder_open_xpm
	};
	int i;

	for (i = 0; i < 2; i++)
		dir_colexp_mini_icons[i].pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)dir_colexp_mini_xpms[i] );
}


/* Correspondence from window_init( ) */
void
dirtree_pass_widget( GtkWidget *tree_w )
{
	dir_tree_w = tree_w;

	/* Connect signal handlers */
	g_signal_connect( G_OBJECT(dir_tree_w), "button_press_event", G_CALLBACK(dirtree_select_cb), NULL );
	g_signal_connect( G_OBJECT(dir_tree_w), "row-collapsed", G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_connect( G_OBJECT(dir_tree_w), "row-expanded", G_CALLBACK(dirtree_expand_cb), NULL );

	dirtree_icons_init( );
}


/* Clears out all entries from the directory tree */
void
dirtree_clear( void )
{
	GtkTreeStore *store = get_tree_store( );

	gtk_tree_store_clear( store );
	dirtree_current_dnode = NULL;
}


/* Adds a new entry to the directory tree */
void
dirtree_entry_new( GNode *dnode )
{
	GtkTreeIter *parent_iter = NULL;
	GtkTreeIter *new_iter;
	const char *name;
	boolean expanded;

	g_assert( NODE_IS_DIR(dnode) );

	parent_iter = DIR_NODE_DESC(dnode->parent)->ctnode;
	if (strlen( NODE_DESC(dnode)->name ) > 0)
		name = NODE_DESC(dnode)->name;
	else
		name = _("/. (root)");
	expanded = g_node_depth( dnode ) <= 2;

	new_iter = gui_ctree_node_add( dir_tree_w, parent_iter, dir_colexp_mini_icons, name, expanded, dnode );
	DIR_NODE_DESC(dnode)->ctnode = new_iter;

	if (parent_iter != NULL && dirtree_entry_expanded( dnode->parent )) {
		/* Select the new entry */
		GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dir_tree_w) );
		gtk_tree_selection_select_iter( sel, new_iter );
		/* Scroll to the new entry */
		GtkTreePath *path = gtk_tree_model_get_path( GTK_TREE_MODEL(get_tree_store( )), new_iter );
		gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(dir_tree_w), path, NULL, FALSE, 0.0, 0.0 );
		gtk_tree_path_free( path );
	}
}


/* Call this after the last call to dirtree_entry_new( ) */
void
dirtree_no_more_entries( void )
{
	/* No freeze/thaw needed with GtkTreeView */
}


/* This updates the directory tree to show (and select) a particular
 * directory entry, repopulating the file list with the contents of the
 * directory if not already listed */
void
dirtree_entry_show( GNode *dnode )
{
	GtkTreeIter *iter;
	GtkTreePath *path;
	GtkTreeSelection *sel;

	g_assert( NODE_IS_DIR(dnode) );

	/* Repopulate file list if directory is different */
	if (dnode != dirtree_current_dnode) {
		filelist_populate( dnode );
		gui_update( );
	}

	iter = DIR_NODE_DESC(dnode)->ctnode;
	if (iter != NULL) {
		path = gtk_tree_model_get_path( GTK_TREE_MODEL(get_tree_store( )), iter );
		if (path != NULL) {
			/* Select the entry */
			sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dir_tree_w) );
			gtk_tree_selection_select_iter( sel, iter );
			/* Scroll to the entry */
			gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(dir_tree_w), path, NULL, TRUE, 0.5, 0.0 );
			gtk_tree_path_free( path );
		}
	}
	else {
		/* No iter - unselect all */
		GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dir_tree_w) );
		gtk_tree_selection_unselect_all( sel );
	}

	dirtree_current_dnode = dnode;
}


/* Returns TRUE if the entry for the given directory is expanded */
boolean
dirtree_entry_expanded( GNode *dnode )
{
	GtkTreeIter *iter;
	GtkTreePath *path;
	boolean expanded;

	g_assert( NODE_IS_DIR(dnode) );

	iter = DIR_NODE_DESC(dnode)->ctnode;
	if (iter == NULL)
		return FALSE;

	path = gtk_tree_model_get_path( GTK_TREE_MODEL(get_tree_store( )), iter );
	if (path == NULL)
		return FALSE;

	expanded = gtk_tree_view_row_expanded( GTK_TREE_VIEW(dir_tree_w), path );
	gtk_tree_path_free( path );

	return expanded;
}


/* Helper function */
static void
block_colexp_handlers( void )
{
	g_signal_handlers_block_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_handlers_block_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_expand_cb), NULL );
}


/* Helper function */
static void
unblock_colexp_handlers( void )
{
	g_signal_handlers_unblock_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_collapse_cb), NULL );
	g_signal_handlers_unblock_by_func( G_OBJECT(dir_tree_w), G_CALLBACK(dirtree_expand_cb), NULL );
}


/* Helper: recursively collapse a row and all its children */
static void
collapse_recursive( GtkTreeView *tree_view, GtkTreeIter *iter )
{
	GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
	GtkTreeIter child;
	GtkTreePath *path;

	/* Collapse children first (depth-first) */
	if (gtk_tree_model_iter_children( model, &child, iter )) {
		do {
			collapse_recursive( tree_view, &child );
		} while (gtk_tree_model_iter_next( model, &child ));
	}

	/* Collapse this row */
	path = gtk_tree_model_get_path( model, iter );
	gtk_tree_view_collapse_row( tree_view, path );
	/* Update icon to collapsed */
	gtk_tree_store_set( GTK_TREE_STORE(model), iter, COL_PIXBUF, dir_colexp_mini_icons[0].pixbuf, -1 );
	gtk_tree_path_free( path );
}


/* Recursively collapses the directory tree entry of the given directory */
void
dirtree_entry_collapse_recursive( GNode *dnode )
{
	GtkTreeIter *iter;

	g_assert( NODE_IS_DIR(dnode) );

	iter = DIR_NODE_DESC(dnode)->ctnode;
	if (iter == NULL)
		return;

	block_colexp_handlers( );
	collapse_recursive( GTK_TREE_VIEW(dir_tree_w), iter );
	unblock_colexp_handlers( );
}


/* Helper: expand a row and update its icon */
static void
expand_row( GtkTreeView *tree_view, GtkTreeIter *iter )
{
	GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
	GtkTreePath *path = gtk_tree_model_get_path( model, iter );

	gtk_tree_view_expand_row( tree_view, path, FALSE );
	gtk_tree_store_set( GTK_TREE_STORE(model), iter, COL_PIXBUF, dir_colexp_mini_icons[1].pixbuf, -1 );
	gtk_tree_path_free( path );
}


/* Expands the directory tree entry of the given directory. If any of its
 * ancestor directory entries are not expanded, then they are expanded
 * as well */
void
dirtree_entry_expand( GNode *dnode )
{
	GNode *up_node;

	g_assert( NODE_IS_DIR(dnode) );

	block_colexp_handlers( );
	up_node = dnode;
	while (NODE_IS_DIR(up_node)) {
		if (!dirtree_entry_expanded( up_node )) {
			GtkTreeIter *iter = DIR_NODE_DESC(up_node)->ctnode;
			if (iter != NULL)
				expand_row( GTK_TREE_VIEW(dir_tree_w), iter );
		}
		up_node = up_node->parent;
	}
	unblock_colexp_handlers( );
}


/* Helper: recursively expand a row and all its children */
static void
expand_recursive( GtkTreeView *tree_view, GtkTreeIter *iter )
{
	GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
	GtkTreeIter child;

	/* Expand this row */
	expand_row( tree_view, iter );

	/* Expand children */
	if (gtk_tree_model_iter_children( model, &child, iter )) {
		do {
			expand_recursive( tree_view, &child );
		} while (gtk_tree_model_iter_next( model, &child ));
	}
}


/* Recursively expands the entire directory tree subtree of the given
 * directory */
void
dirtree_entry_expand_recursive( GNode *dnode )
{
	GtkTreeIter *iter;

	g_assert( NODE_IS_DIR(dnode) );

#if DEBUG
	/* Guard against expansions inside collapsed subtrees */
	if (NODE_IS_DIR(dnode->parent))
		g_assert( dirtree_entry_expanded( dnode->parent ) );
#endif

	iter = DIR_NODE_DESC(dnode)->ctnode;
	if (iter == NULL)
		return;

	block_colexp_handlers( );
	expand_recursive( GTK_TREE_VIEW(dir_tree_w), iter );
	unblock_colexp_handlers( );
}


/* end dirtree.c */
