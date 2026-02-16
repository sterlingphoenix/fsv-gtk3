/* gui.c */

/* Higher-level GTK+ interface */

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
#include <gtk/gtk.h>
#include "gui.h"

#include "animation.h"
#include "ogl.h" /* ogl_widget_new( ) */


/* Box packing flags */
enum {
	GUI_PACK_EXPAND	= 1 << 0,
	GUI_PACK_FILL	= 1 << 1,
	GUI_PACK_START	= 1 << 2
};


/* For whenever gtk_main( ) is far away */
void
gui_update( void )
{
	while (gtk_events_pending( ) > 0)
		gtk_main_iteration( );
}


/* This checks if the widget associated with the given adjustment is
 * currently busy redrawing/reconfiguring itself, or is in steady state
 * (this is used when animating widgets to avoid changing the adjustment
 * too often, otherwise the widget can't keep up and things slow down) */
boolean
gui_adjustment_widget_busy( GtkAdjustment *adj )
{
	static const double threshold = (1.0 / 18.0);
	double t_prev;
	double t_now;
	double *tp;

	/* ---- HACK ALERT ----
	 * This doesn't actually check GTK+ internals-- I'm not sure which
	 * ones are relevant here. This just checks the amount of time that
	 * has passed since the last time the function was called with the
	 * same adjustment and returned FALSE, and if it's below a certain
	 * threshold, the object is considered "busy" (returning TRUE) */

	t_now = xgettime( );

	tp = g_object_get_data( G_OBJECT(adj), "t_prev" );
	if (tp == NULL) {
		tp = NEW(double);
		*tp = t_now;
		g_object_set_data_full( G_OBJECT(adj), "t_prev", tp, _xfree );
		return FALSE;
	}

	t_prev = *tp;

	if ((t_now - t_prev) > threshold) {
		*tp = t_now;
		return FALSE;
	}

	return TRUE;
}


/* Step/end callback used in animating a GtkAdjustment */
static void
adjustment_step_cb( Morph *morph )
{
	GtkAdjustment *adj;
	double anim_value;

	adj = (GtkAdjustment *)morph->data;
	g_return_if_fail( GTK_IS_ADJUSTMENT(adj) );
	anim_value = *(morph->var);
	if (!gui_adjustment_widget_busy( adj ) || (ABS(morph->end_value - anim_value) < EPSILON))
		gtk_adjustment_set_value( adj, anim_value );
}


/* Creates an integer-valued adjustment */
GtkAdjustment *
gui_int_adjustment( int value, int lower, int upper )
{
	return (GtkAdjustment *)gtk_adjustment_new( (float)value, (float)lower, (float)upper, 1.0, 1.0, 1.0 );
}


/* This places child_w into parent_w intelligently. expand and fill
 * flags are applicable only if parent_w is a box widget */
static void
parent_child_full( GtkWidget *parent_w, GtkWidget *child_w, boolean expand, boolean fill )
{
	bitfield *packing_flags;
	boolean start = TRUE;

	if (parent_w != NULL) {
		if (GTK_IS_BOX(parent_w)) {
			packing_flags = g_object_get_data( G_OBJECT(parent_w), "packing_flags" );
			if (packing_flags != NULL) {
                                /* Get (non-default) box-packing flags */
				expand = *packing_flags & GUI_PACK_EXPAND;
				fill = *packing_flags & GUI_PACK_FILL;
				start = *packing_flags & GUI_PACK_START;
			}
                        if (start)
				gtk_box_pack_start( GTK_BOX(parent_w), child_w, expand, fill, 0 );
                        else
				gtk_box_pack_end( GTK_BOX(parent_w), child_w, expand, fill, 0 );
		}
		else
			gtk_container_add( GTK_CONTAINER(parent_w), child_w );
		gtk_widget_show( child_w );
	}
}


/* Calls parent_child_full( ) with defaults */
static void
parent_child( GtkWidget *parent_w, GtkWidget *child_w )
{
	parent_child_full( parent_w, child_w, NO_EXPAND, NO_FILL );
}


/* The horizontal box widget */
GtkWidget *
gui_hbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *hbox_w;

	hbox_w = gtk_hbox_new( FALSE, spacing );
	gtk_container_set_border_width( GTK_CONTAINER(hbox_w), spacing );
	parent_child( parent_w, hbox_w );

	return hbox_w;
}


/* The vertical box widget */
GtkWidget *
gui_vbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *vbox_w;

	vbox_w = gtk_vbox_new( FALSE, spacing );
	gtk_container_set_border_width( GTK_CONTAINER(vbox_w), spacing );
	parent_child( parent_w, vbox_w );

	return vbox_w;
}


/* Changes a box widget's default packing flags (i.e. the flags that will
 * be used to pack subsequent children) */
void
gui_box_set_packing( GtkWidget *box_w, boolean expand, boolean fill, boolean start )
{
	static const char data_key[] = "packing_flags";
	bitfield *packing_flags;

	/* Make sure box_w is a box widget */
	g_assert( GTK_IS_BOX(box_w) );
	/* If expand is FALSE, then fill should not be TRUE */
	g_assert( expand || !fill );

	packing_flags = g_object_get_data( G_OBJECT(box_w), data_key );
	if (packing_flags == NULL) {
		/* Allocate new packing-flags variable for box */
		packing_flags = NEW(bitfield);
		g_object_set_data_full( G_OBJECT(box_w), data_key, packing_flags, _xfree );
	}

        /* Set flags appropriately */
	*packing_flags = 0;
	*packing_flags |= (expand ? GUI_PACK_EXPAND : 0);
	*packing_flags |= (fill ? GUI_PACK_FILL : 0);
	*packing_flags |= (start ? GUI_PACK_START : 0);
}


/* The standard button widget */
GtkWidget *
gui_button_add( GtkWidget *parent_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *button_w;

	button_w = gtk_button_new( );
	if (label != NULL)
		gui_label_add( button_w, label );
	g_signal_connect( G_OBJECT(button_w), "clicked", G_CALLBACK(callback), callback_data );
	parent_child( parent_w, button_w );

	return button_w;
}


/* Creates a button with a pixmap prepended to the label */
GtkWidget *
gui_button_with_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *button_w;
	GtkWidget *hbox_w, *hbox2_w;

	button_w = gtk_button_new( );
	parent_child( parent_w, button_w );
	hbox_w = gui_hbox_add( button_w, 0 );
	hbox2_w = gui_hbox_add( hbox_w, 0 );
	gui_widget_packing( hbox2_w, EXPAND, NO_FILL, AT_START );
	gui_pixmap_xpm_add( hbox2_w, xpm_data );
	if (label != NULL) {
		gui_vbox_add( hbox2_w, 2 ); /* spacer */
		gui_label_add( hbox2_w, label );
	}
	g_signal_connect( G_OBJECT(button_w), "clicked", G_CALLBACK(callback), callback_data );

	return button_w;
}


/* The toggle button widget */
GtkWidget *
gui_toggle_button_add( GtkWidget *parent_w, const char *label, boolean active, void (*callback)( ), void *callback_data )
{
	GtkWidget *tbutton_w;

	tbutton_w = gtk_toggle_button_new( );
	if (label != NULL)
		gui_label_add( tbutton_w, label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tbutton_w), active );
	g_signal_connect( G_OBJECT(tbutton_w), "toggled", G_CALLBACK(callback), callback_data );
	parent_child( parent_w, tbutton_w );

	return tbutton_w;
}


/* The [multi-column] list widget (fitted into a scrolled window).
 * Returns a GtkTreeView backed by a GtkListStore.
 * Model columns: pixbuf (0), text[0..num_cols-1] (1..num_cols), pointer (num_cols+1).
 * First visible column shows pixbuf + text; remaining columns show text only. */
GtkWidget *
gui_clist_add( GtkWidget *parent_w, int num_cols, char *col_titles[] )
{
	GtkWidget *scrollwin_w;
	GtkWidget *tree_view_w;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GType *col_types;
	int total_cols;
	int i;
	int *num_cols_p;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Build column types array: pixbuf, num_cols strings, pointer */
	total_cols = num_cols + 2;
	col_types = g_new( GType, total_cols );
	col_types[0] = GDK_TYPE_PIXBUF;
	for (i = 0; i < num_cols; i++)
		col_types[i + 1] = G_TYPE_STRING;
	col_types[num_cols + 1] = G_TYPE_POINTER;

	store = gtk_list_store_newv( total_cols, col_types );
	g_free( col_types );

	tree_view_w = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
	g_object_unref( store );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW(tree_view_w), col_titles != NULL );

	/* First column: pixbuf + text */
	column = gtk_tree_view_column_new( );
	if (col_titles != NULL && col_titles[0] != NULL)
		gtk_tree_view_column_set_title( column, col_titles[0] );
	renderer = gtk_cell_renderer_pixbuf_new( );
	gtk_tree_view_column_pack_start( column, renderer, FALSE );
	gtk_tree_view_column_add_attribute( column, renderer, "pixbuf", 0 );
	renderer = gtk_cell_renderer_text_new( );
	gtk_tree_view_column_pack_start( column, renderer, TRUE );
	gtk_tree_view_column_add_attribute( column, renderer, "text", 1 );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW(tree_view_w), column );

	/* Additional text columns */
	for (i = 1; i < num_cols; i++) {
		renderer = gtk_cell_renderer_text_new( );
		column = gtk_tree_view_column_new_with_attributes(
			(col_titles != NULL) ? col_titles[i] : NULL,
			renderer, "text", i + 1, NULL );
		gtk_tree_view_column_set_resizable( column, TRUE );
		gtk_tree_view_append_column( GTK_TREE_VIEW(tree_view_w), column );
	}

	/* Single selection mode */
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW(tree_view_w) );
	gtk_tree_selection_set_mode( selection, GTK_SELECTION_SINGLE );

	/* Store num_cols on the widget for later use */
	num_cols_p = g_new( int, 1 );
	*num_cols_p = num_cols;
	g_object_set_data_full( G_OBJECT(tree_view_w), "num_cols", num_cols_p, g_free );

	gtk_container_add( GTK_CONTAINER(scrollwin_w), tree_view_w );
	gtk_widget_show( tree_view_w );

	return tree_view_w;
}


/* Scrolls a tree view to a given row (-1 indicates last row).
 * For instant scroll (moveto_time <= 0), uses gtk_tree_view_scroll_to_cell.
 * For animated scroll, morphs the scrolled window's vadjustment.
 * WARNING: This implementation does not gracefully handle multiple
 * animated scrolls on the same tree view! */
void
gui_clist_moveto_row( GtkWidget *tree_view_w, int row, double moveto_time )
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkAdjustment *vadj;
	GtkWidget *scrollwin_w;
	double *anim_value_var;
	float k, new_value;
	int n_rows;

	model = gtk_tree_view_get_model( GTK_TREE_VIEW(tree_view_w) );
	n_rows = gtk_tree_model_iter_n_children( model, NULL );
	if (n_rows == 0)
		return;

	if (row < 0)
		row = n_rows - 1;
	if (row >= n_rows)
		row = n_rows - 1;

	if (moveto_time <= 0.0) {
		/* Instant scroll */
		path = gtk_tree_path_new_from_indices( row, -1 );
		gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(tree_view_w), path, NULL, TRUE, 0.5, 0.0 );
		gtk_tree_path_free( path );
		return;
	}

	/* Animated scroll using vadjustment */
	scrollwin_w = gtk_widget_get_parent( tree_view_w );
	if (!GTK_IS_SCROLLED_WINDOW(scrollwin_w))
		return;
	vadj = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(scrollwin_w) );

	k = (double)row / (double)n_rows;
	k = k * vadj->upper - 0.5 * vadj->page_size;
	new_value = CLAMP(k, 0.0, vadj->upper - vadj->page_size);

	/* Allocate an external value variable if adjustment doesn't
	 * already have one */
	anim_value_var = g_object_get_data( G_OBJECT(vadj), "anim_value_var" );
	if (anim_value_var == NULL) {
		anim_value_var = NEW(double);
		g_object_set_data_full( G_OBJECT(vadj), "anim_value_var", anim_value_var, _xfree );
	}

	/* If already scrolling, stop it */
	morph_break( anim_value_var );

	/* Begin animation */
	*anim_value_var = vadj->value;
	morph_full( anim_value_var, MORPH_SIGMOID, new_value, moveto_time, adjustment_step_cb, adjustment_step_cb, vadj );
}


/* Internal callback for the color picker widget.
 * GtkColorButton "color-set" signal passes (widget, user_data) only. */
static void
color_picker_cb( GtkColorButton *colorbutton, gpointer data )
{
	void (*user_callback)( RGBcolor *, void * );
	RGBcolor color;
	GdkColor gdk_color;

	gtk_color_button_get_color( colorbutton, &gdk_color );
	color.r = (float)gdk_color.red / 65535.0;
	color.g = (float)gdk_color.green / 65535.0;
	color.b = (float)gdk_color.blue / 65535.0;

	/* Call user callback */
	user_callback = (void (*)( RGBcolor *, void * ))g_object_get_data( G_OBJECT(colorbutton), "user_callback" );
	(user_callback)( &color, data );
}


/* The color picker widget. Color is initialized to the one given, and the
 * color selection dialog will have the specified title when brought up.
 * Changing the color (i.e. pressing OK in the color selection dialog)
 * activates the given callback */
GtkWidget *
gui_colorpicker_add( GtkWidget *parent_w, RGBcolor *init_color, const char *title, void (*callback)( ), void *callback_data )
{
	GtkWidget *colorbutton_w;

	colorbutton_w = gtk_color_button_new();
	gui_colorpicker_set_color(colorbutton_w, init_color);
	gtk_color_button_set_title(GTK_COLOR_BUTTON(colorbutton_w), title);
	g_signal_connect(G_OBJECT(colorbutton_w), "color-set", G_CALLBACK(color_picker_cb), callback_data);
	g_object_set_data(G_OBJECT(colorbutton_w), "user_callback", (void *)callback);
	parent_child(parent_w, colorbutton_w);

	return colorbutton_w;
}


/* Sets the color on a color picker widget */
void
gui_colorpicker_set_color( GtkWidget *colorbutton_w, RGBcolor *color )
{
	GdkColor gdk_color = {
		.red	= color->r * 65535,
		.green	= color->g * 65535,
		.blue	= color->b * 65535,
	};

	gtk_color_button_set_color(GTK_COLOR_BUTTON(colorbutton_w), &gdk_color);
}


/* Column indices for the tree (ctree replacement) model */
enum {
	CTREE_COL_PIXBUF,	/* GdkPixbuf - node icon */
	CTREE_COL_NAME,		/* gchararray - display name */
	CTREE_COL_DATA,		/* gpointer - user data (GNode *) */
	CTREE_NUM_COLS
};


/* The tree widget (fitted into a scrolled window).
 * Returns a GtkTreeView backed by a GtkTreeStore. */
GtkWidget *
gui_ctree_add( GtkWidget *parent_w )
{
	GtkWidget *scrollwin_w;
	GtkWidget *tree_view_w;
	GtkTreeStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the tree store and tree view */
	store = gtk_tree_store_new( CTREE_NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER );
	tree_view_w = gtk_tree_view_new_with_model( GTK_TREE_MODEL(store) );
	g_object_unref( store );

	/* Single column: icon + name */
	column = gtk_tree_view_column_new( );
	renderer = gtk_cell_renderer_pixbuf_new( );
	gtk_tree_view_column_pack_start( column, renderer, FALSE );
	gtk_tree_view_column_add_attribute( column, renderer, "pixbuf", CTREE_COL_PIXBUF );
	renderer = gtk_cell_renderer_text_new( );
	gtk_tree_view_column_pack_start( column, renderer, TRUE );
	gtk_tree_view_column_add_attribute( column, renderer, "text", CTREE_COL_NAME );
	gtk_tree_view_append_column( GTK_TREE_VIEW(tree_view_w), column );

	/* Tree appearance */
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW(tree_view_w), FALSE );
	gtk_tree_view_set_enable_tree_lines( GTK_TREE_VIEW(tree_view_w), TRUE );

	/* Browse selection mode */
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW(tree_view_w) );
	gtk_tree_selection_set_mode( selection, GTK_SELECTION_BROWSE );

	gtk_container_add( GTK_CONTAINER(scrollwin_w), tree_view_w );
	gtk_widget_show( tree_view_w );

	return tree_view_w;
}


/* Adds a new node to a tree view backed by a GtkTreeStore.
 * Returns a heap-allocated GtkTreeIter (caller stores in ctnode field).
 * icon_pair[0] = collapsed icon, icon_pair[1] = expanded icon.
 * The collapsed icon is displayed initially; expand/collapse callbacks
 * swap icons as needed. */
GtkTreeIter *
gui_ctree_node_add( GtkWidget *tree_w, GtkTreeIter *parent, Icon icon_pair[2], const char *text, boolean expanded, void *data )
{
	GtkTreeStore *store;
	GtkTreeIter *iter;
	GdkPixbuf *icon;

	store = GTK_TREE_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(tree_w) ));
	iter = g_new( GtkTreeIter, 1 );
	gtk_tree_store_append( store, iter, parent );

	/* Use collapsed or expanded icon based on initial state */
	icon = expanded ? icon_pair[1].pixbuf : icon_pair[0].pixbuf;
	gtk_tree_store_set( store, iter,
		CTREE_COL_PIXBUF, icon,
		CTREE_COL_NAME, text,
		CTREE_COL_DATA, data,
		-1 );

	/* Expand the row if requested */
	if (expanded) {
		GtkTreePath *path = gtk_tree_model_get_path( GTK_TREE_MODEL(store), iter );
		gtk_tree_view_expand_row( GTK_TREE_VIEW(tree_w), path, FALSE );
		gtk_tree_path_free( path );
	}

	return iter;
}


/* Changes the mouse cursor glyph associated with the given widget.
 * A glyph of -1 indicates the default cursor */
void
gui_cursor( GtkWidget *widget, int glyph )
{
	GdkCursor *prev_cursor, *cursor;
	int *prev_glyph;

	/* Get cursor information from widget */
	prev_cursor = g_object_get_data( G_OBJECT(widget), "gui_cursor" );
        prev_glyph = g_object_get_data( G_OBJECT(widget), "gui_glyph" );

	if (prev_glyph == NULL) {
		if (glyph < 0)
			return; /* default cursor is already set */
                /* First-time setup */
		prev_glyph = NEW(int);
		g_object_set_data_full( G_OBJECT(widget), "gui_glyph", prev_glyph, _xfree );
	}
	else {
		/* Check if requested glyph is same as previous one */
		if (glyph == *prev_glyph)
			return;
	}

	/* Create new cursor and make it active */
	if (glyph >= 0)
		cursor = gdk_cursor_new_for_display( gtk_widget_get_display( widget ), (GdkCursorType)glyph );
	else
		cursor = NULL;
	gdk_window_set_cursor( gtk_widget_get_window( widget ), cursor );

	/* Don't need the old cursor anymore */
	if (prev_cursor != NULL)
		gdk_cursor_unref( prev_cursor );

	if (glyph >= 0) {
		/* Save new cursor information */
		g_object_set_data( G_OBJECT(widget), "gui_cursor", cursor );
		*prev_glyph = glyph;
	}
	else {
		/* Clean up after ourselves */
		g_object_set_data( G_OBJECT(widget), "gui_cursor", NULL );
		g_object_set_data( G_OBJECT(widget), "gui_glyph", NULL );
	}
}


/* The date edit widget (imported from Gnomeland). The given callback is
 * called whenever the date/time is changed */
GtkWidget *
gui_dateedit_add( GtkWidget *parent_w, time_t the_time, void (*callback)( ), void *callback_data )
{
	GtkWidget *dateedit_w;

	/* Date edit was a GNOME widget, not available in plain GTK+.
	 * Return a placeholder label instead */
	dateedit_w = gtk_label_new( "N/A" );
	parent_child( parent_w, dateedit_w );

	return dateedit_w;
}


/* Reads current time from a date edit widget */
time_t
gui_dateedit_get_time( GtkWidget *dateedit_w )
{
	//return gnome_date_edit_get_date( GNOME_DATE_EDIT(dateedit_w) );
	return 0;
}


/* Sets the time on a date edit widget */
void
gui_dateedit_set_time( GtkWidget *dateedit_w, time_t the_time )
{
	//gnome_date_edit_set_time( GNOME_DATE_EDIT(dateedit_w), the_time );
}


/* The entry (text input) widget */
GtkWidget *
gui_entry_add( GtkWidget *parent_w, const char *init_text, void (*callback)( ), void *callback_data )
{
	GtkWidget *entry_w;

	entry_w = gtk_entry_new( );
        if (init_text != NULL)
		gtk_entry_set_text( GTK_ENTRY(entry_w), init_text );
	if (callback != NULL )
		g_signal_connect( G_OBJECT(entry_w), "activate", G_CALLBACK(callback), callback_data );
	parent_child_full( parent_w, entry_w, EXPAND, FILL );

	return entry_w;
}


/* Sets the text in an entry to the specified string */
void
gui_entry_set_text( GtkWidget *entry_w, const char *entry_text )
{
	gtk_entry_set_text( GTK_ENTRY(entry_w), entry_text );
}


/* Returns the text currently in an entry */
char *
gui_entry_get_text( GtkWidget *entry_w )
{
	return (char *)gtk_entry_get_text( GTK_ENTRY(entry_w) );
}


/* Highlights the text in an entry */
void
gui_entry_highlight( GtkWidget *entry_w )
{
	gtk_entry_select_region( GTK_ENTRY(entry_w), 0, GTK_ENTRY(entry_w)->text_length );
}


/* The frame widget (with optional title) */
GtkWidget *
gui_frame_add( GtkWidget *parent_w, const char *title )
{
	GtkWidget *frame_w;

	frame_w = gtk_frame_new( title );
	parent_child_full( parent_w, frame_w, EXPAND, FILL );

	return frame_w;
}


/* The OpenGL area widget */
GtkWidget *
gui_gl_area_add( GtkWidget *parent_w )
{
	GtkWidget *gl_area_w;
	int bitmask = 0;

	gl_area_w = ogl_widget_new( );
	bitmask |= GDK_EXPOSURE_MASK;
	bitmask |= GDK_POINTER_MOTION_MASK;
	bitmask |= GDK_BUTTON_MOTION_MASK;
	bitmask |= GDK_BUTTON1_MOTION_MASK;
	bitmask |= GDK_BUTTON2_MOTION_MASK;
	bitmask |= GDK_BUTTON3_MOTION_MASK;
	bitmask |= GDK_BUTTON_PRESS_MASK;
	bitmask |= GDK_BUTTON_RELEASE_MASK;
	bitmask |= GDK_LEAVE_NOTIFY_MASK;
	bitmask |= GDK_SCROLL_MASK;
	bitmask |= GDK_STRUCTURE_MASK;
	gtk_widget_set_events( GTK_WIDGET(gl_area_w), bitmask );
	parent_child_full( parent_w, gl_area_w, EXPAND, FILL );

	return gl_area_w;
}


/* Sets up keybindings (accelerators). Call this any number of times with
 * widget/keystroke pairs, and when all have been specified, call with the
 * parent window widget (and no keystroke) to attach the keybindings.
 * Keystroke syntax: "K" == K keypress, "^K" == Ctrl-K */
void
gui_keybind( GtkWidget *widget, char *keystroke )
{
	static GtkAccelGroup *accel_group = NULL;
	int mods;
	char key;

	if (accel_group == NULL)
		accel_group = gtk_accel_group_new( );

	if (GTK_IS_WINDOW(widget)) {
		/* Attach keybindings */
		gtk_window_add_accel_group(GTK_WINDOW(widget), accel_group);
		accel_group = NULL;
		return;
	}

	/* Parse keystroke string */
	switch (keystroke[0]) {
		case '^':
		/* Ctrl-something keystroke specified */
		mods = GDK_CONTROL_MASK;
		key = keystroke[1];
		break;

		default:
		/* Simple keypress */
		mods = 0;
		key = keystroke[0];
		break;
	}

	if (GTK_IS_MENU_ITEM(widget)) {
		gtk_widget_add_accelerator( widget, "activate", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}
	if (GTK_IS_BUTTON(widget)) {
		gtk_widget_add_accelerator( widget, "clicked", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}

	/* Make widget grab focus when its key is pressed */
	gtk_widget_add_accelerator( widget, "grab_focus", accel_group, key, mods, GTK_ACCEL_VISIBLE );
}


/* The label widget */
GtkWidget *
gui_label_add( GtkWidget *parent_w, const char *label_text )
{
	GtkWidget *label_w;
	GtkWidget *hbox_w;

	label_w = gtk_label_new( label_text );
	if (parent_w != NULL) {
		if (GTK_IS_BUTTON(parent_w)) {
			/* Labels are often too snug inside buttons */
			hbox_w = gui_hbox_add( parent_w, 0 );
			gtk_box_pack_start( GTK_BOX(hbox_w), label_w, TRUE, FALSE, 5 );
			gtk_widget_show( label_w );
		}
		else
			parent_child( parent_w, label_w );
	}

	return label_w;
}


/* Adds a menu to a menu bar, or a submenu to a menu */
GtkWidget *
gui_menu_add( GtkWidget *parent_menu_w, const char *label )
{
	GtkWidget *menu_item_w;
	GtkWidget *menu_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	/* parent_menu can be a menu bar or a regular menu */
	if (GTK_IS_MENU_BAR(parent_menu_w))
		gtk_menu_shell_append( GTK_MENU_SHELL(parent_menu_w), menu_item_w );
	else
		gtk_menu_shell_append( GTK_MENU_SHELL(parent_menu_w), menu_item_w );
	gtk_widget_show( menu_item_w );
	menu_w = gtk_menu_new( );
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu_item_w), menu_w );
	/* Bug in GTK+? Following pointer shouldn't be NULL */
	GTK_MENU(menu_w)->parent_menu_item = menu_item_w;

	return menu_w;
}


/* Adds a menu item to a menu */
GtkWidget *
gui_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *menu_item_w;
	menu_item_w = gtk_menu_item_new_with_label( label );
	gtk_menu_shell_append( GTK_MENU_SHELL(menu_w), menu_item_w );
	if (callback != NULL)
		g_signal_connect( G_OBJECT(menu_item_w), "activate", G_CALLBACK(callback), callback_data );
	gtk_widget_show( menu_item_w );

	return menu_item_w;
}


/* This initiates the definition of a radio menu item group. The item in
 * the specified position will be the one that is initially selected
 * (0 == first, 1 == second, and so on) */
void
gui_radio_menu_begin( int init_selected )
{
	gui_radio_menu_item_add( NULL, NULL, NULL, &init_selected );
}


/* Adds a radio menu item to a menu. Don't forget to call
 * gui_radio_menu_begin( ) first.
 * WARNING: When the initially selected menu item is set, the first item
 * in the group will be "toggled" off. The callback should either watch
 * for this, or do nothing if the widget's "active" flag is FALSE */
GtkWidget *
gui_radio_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	static GSList *radio_group;
	static int init_selected;
	static int radmenu_item_num;
	GtkWidget *radmenu_item_w = NULL;

	if (menu_w == NULL) {
		/* We're being called from begin_radio_menu_group( ) */
		radio_group = NULL;
		radmenu_item_num = 0;
		init_selected = *((int *)callback_data);
	}
	else {
		radmenu_item_w = gtk_radio_menu_item_new_with_label( radio_group, label );
		radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(radmenu_item_w) );
		gtk_menu_shell_append( GTK_MENU_SHELL(menu_w), radmenu_item_w );
		if (radmenu_item_num == init_selected)
			gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(radmenu_item_w), TRUE );
		g_signal_connect( G_OBJECT(radmenu_item_w), "toggled", G_CALLBACK(callback), callback_data );
		gtk_widget_show( radmenu_item_w );
		++radmenu_item_num;
	}

	return radmenu_item_w;
}


/* Storage for option menu items being built */
struct OptionMenuItem {
	const char *label;
	void (*callback)( );
	void *callback_data;
};
static struct OptionMenuItem optmenu_items[16];
static int optmenu_item_count = 0;


/* Callback dispatcher for combo box "changed" signal */
static void
option_menu_changed_cb( GtkComboBox *combo, gpointer data )
{
	struct OptionMenuItem *items;
	int count;
	int active;

	active = gtk_combo_box_get_active( combo );
	items = (struct OptionMenuItem *)g_object_get_data( G_OBJECT(combo), "optmenu_items" );
	count = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(combo), "optmenu_item_count" ));
	if (active >= 0 && active < count && items[active].callback != NULL) {
		void (*cb)( GtkWidget *, void * ) = (void (*)( GtkWidget *, void * ))items[active].callback;
		cb( GTK_WIDGET(combo), items[active].callback_data );
	}
}


/* The combo box widget. Options must have already been defined using
 * gui_option_menu_item( ) */
GtkWidget *
gui_option_menu_add( GtkWidget *parent_w, int init_selected )
{
	GtkWidget *combo_w;
	struct OptionMenuItem *items_copy;
	int i;

	combo_w = gtk_combo_box_text_new( );
	for (i = 0; i < optmenu_item_count; i++)
		gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo_w), optmenu_items[i].label );

	/* Store item data on the combo box for the callback dispatcher */
	items_copy = g_new( struct OptionMenuItem, optmenu_item_count );
	memcpy( items_copy, optmenu_items, optmenu_item_count * sizeof(struct OptionMenuItem) );
	g_object_set_data_full( G_OBJECT(combo_w), "optmenu_items", items_copy, g_free );
	g_object_set_data( G_OBJECT(combo_w), "optmenu_item_count", GINT_TO_POINTER(optmenu_item_count) );

	gtk_combo_box_set_active( GTK_COMBO_BOX(combo_w), init_selected );
	g_signal_connect( G_OBJECT(combo_w), "changed", G_CALLBACK(option_menu_changed_cb), NULL );

	parent_child( parent_w, combo_w );
	optmenu_item_count = 0;

	return combo_w;
}


/* Combo box item definition. Call this once for each menu item, and then call
 * gui_option_menu_add( ) to produce the finished widget */
GtkWidget *
gui_option_menu_item( const char *label, void (*callback)( ), void *callback_data )
{
	g_assert( optmenu_item_count < 16 );
	optmenu_items[optmenu_item_count].label = label;
	optmenu_items[optmenu_item_count].callback = callback;
	optmenu_items[optmenu_item_count].callback_data = callback_data;
	optmenu_item_count++;

	/* Return value is no longer meaningful (was GtkMenuItem*) */
	return NULL;
}


/* The notebook widget */
GtkWidget *
gui_notebook_add( GtkWidget *parent_w )
{
	GtkWidget *notebook_w;

	notebook_w = gtk_notebook_new( );
	parent_child_full( parent_w, notebook_w, EXPAND, FILL );

	return notebook_w;
}


/* Adds a new page to a notebook, with the given tab label, and whose
 * content is defined by the given widget */
void
gui_notebook_page_add( GtkWidget *notebook_w, const char *tab_label, GtkWidget *content_w )
{
	GtkWidget *tab_label_w;

	tab_label_w = gtk_label_new( tab_label );
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook_w), content_w, tab_label_w );
	gtk_widget_show( tab_label_w );
	gtk_widget_show( content_w );
}


/* Horizontal paned window widget */
GtkWidget *
gui_hpaned_add( GtkWidget *parent_w, int divider_x_pos )
{
	GtkWidget *hpaned_w;

	hpaned_w = gtk_hpaned_new( );
	gtk_paned_set_position( GTK_PANED(hpaned_w), divider_x_pos );
	parent_child_full( parent_w, hpaned_w, EXPAND, FILL );

	return hpaned_w;
}


/* Vertical paned window widget */
GtkWidget *
gui_vpaned_add( GtkWidget *parent_w, int divider_y_pos )
{
	GtkWidget *vpaned_w;

	vpaned_w = gtk_vpaned_new( );
	gtk_paned_set_position( GTK_PANED(vpaned_w), divider_y_pos );
	parent_child_full( parent_w, vpaned_w, EXPAND, FILL );

	return vpaned_w;
}


/* The image widget (created from XPM data) */
GtkWidget *
gui_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data )
{
	GtkWidget *image_w;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)xpm_data );
	image_w = gtk_image_new_from_pixbuf( pixbuf );
	g_object_unref( pixbuf );
	parent_child( parent_w, image_w );

	return image_w;
}


/* The color preview widget (GtkDrawingArea) */
GtkWidget *
gui_preview_add( GtkWidget *parent_w )
{
	GtkWidget *drawing_w;

	drawing_w = gtk_drawing_area_new( );
	gtk_widget_set_size_request( drawing_w, 64, 16 );
	parent_child_full( parent_w, drawing_w, EXPAND, FILL );

	return drawing_w;
}


/* Expose/draw callback for the spectrum drawing area */
static gboolean
preview_spectrum_expose_cb( GtkWidget *drawing_w, GdkEventExpose *event, gpointer data )
{
	RGBcolor (*spectrum_func)( double x );
	RGBcolor color;
	cairo_t *cr;
	int width, height;
	int i;

	spectrum_func = (RGBcolor (*)( double x ))g_object_get_data( G_OBJECT(drawing_w), "spectrum_func" );
	if (spectrum_func == NULL)
		return FALSE;

	{
		GtkAllocation alloc;
		gtk_widget_get_allocation( drawing_w, &alloc );
		width = alloc.width;
		height = alloc.height;
	}
	if (width <= 0 || height <= 0)
		return FALSE;

	cr = gdk_cairo_create( gtk_widget_get_window( drawing_w ) );

	/* Draw spectrum as vertical 1-pixel-wide stripes */
	for (i = 0; i < width; i++) {
		color = (spectrum_func)( (double)i / (double)(width - 1) );
		cairo_set_source_rgb( cr, color.r, color.g, color.b );
		cairo_rectangle( cr, i, 0, 1, height );
		cairo_fill( cr );
	}

	cairo_destroy( cr );
	return FALSE;
}


/* Fills a preview widget with an arbitrary spectrum. Second argument
 * should be a function returning the appropriate color at a specified
 * fractional position in the spectrum */
void
gui_preview_spectrum( GtkWidget *preview_w, RGBcolor (*spectrum_func)( double x ) )
{
	static const char data_key[] = "spectrum_func";
	boolean first_time;

	/* Check if this is first-time initialization */
	first_time = g_object_get_data( G_OBJECT(preview_w), data_key ) == NULL;

	/* Attach spectrum function to drawing area widget */
	g_object_set_data( G_OBJECT(preview_w), data_key, (void *)spectrum_func );

	if (first_time) {
		g_signal_connect( G_OBJECT(preview_w), "expose_event", G_CALLBACK(preview_spectrum_expose_cb), NULL );
	}

	/* Trigger redraw */
	gtk_widget_queue_draw( preview_w );
}


/* The horizontal scrollbar widget */
GtkWidget *
gui_hscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *hscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	hscrollbar_w = gtk_hscrollbar_new( adjustment );
	gtk_container_add( GTK_CONTAINER(frame_w), hscrollbar_w );
	gtk_widget_show( hscrollbar_w );

	return hscrollbar_w;
}


/* The vertical scrollbar widget */
GtkWidget *
gui_vscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *vscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	vscrollbar_w = gtk_vscrollbar_new( adjustment );
	gtk_container_add( GTK_CONTAINER(frame_w), vscrollbar_w );
	gtk_widget_show( vscrollbar_w );

	return vscrollbar_w;
}


/* The (ever-ubiquitous) separator widget */
GtkWidget *
gui_separator_add( GtkWidget *parent_w )
{
	GtkWidget *separator_w;

	if (parent_w != NULL) {
		if (GTK_IS_MENU(parent_w)) {
			separator_w = gtk_menu_item_new( );
			gtk_menu_shell_append( GTK_MENU_SHELL(parent_w), separator_w );
		}
		else {
			separator_w = gtk_hseparator_new( );
			gtk_box_pack_start( GTK_BOX(parent_w), separator_w, FALSE, FALSE, 10 );
		}
		gtk_widget_show( separator_w );
	}
	else
		separator_w = gtk_hseparator_new( );

	return separator_w;
}


/* The statusbar widget */
GtkWidget *
gui_statusbar_add( GtkWidget *parent_w )
{
	GtkWidget *statusbar_w;

	statusbar_w = gtk_statusbar_new( );
	parent_child( parent_w, statusbar_w );

	return statusbar_w;
}


/* Displays the given message in the given statusbar widget */
void
gui_statusbar_message( GtkWidget *statusbar_w, const char *message )
{
	char strbuf[1024];

	if (GTK_STATUSBAR(statusbar_w)->messages == NULL)
		gtk_statusbar_push( GTK_STATUSBAR(statusbar_w), 1, "" );

	gtk_statusbar_pop( GTK_STATUSBAR(statusbar_w), 1 );
	/* Prefix a space so that text doesn't touch left edge */
	snprintf( strbuf, sizeof(strbuf), " %s", message );
	gtk_statusbar_push( GTK_STATUSBAR(statusbar_w), 1, strbuf );
}


/* The table (layout) widget */
GtkWidget *
gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding )
{
	GtkWidget *table_w;
	int *cp;

	table_w = gtk_table_new( num_rows, num_cols, homog );
	cp = NEW(int);
	*cp = cell_padding;
        g_object_set_data_full( G_OBJECT(table_w), "cell_padding", cp, _xfree );
	parent_child_full( parent_w, table_w, EXPAND, FILL );

	return table_w;
}


/* Attaches a widget to a table */
void
gui_table_attach( GtkWidget *table_w, GtkWidget *widget, int left, int right, int top, int bottom )
{
	int cp;

	cp = *(int *)g_object_get_data( G_OBJECT(table_w), "cell_padding" );
	gtk_table_attach( GTK_TABLE(table_w), widget, left, right, top, bottom, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, cp, cp );
	gtk_widget_show( widget );
}


/* The text (area) widget, optionally initialized with text */
GtkWidget *
gui_text_area_add( GtkWidget *parent_w, const char *init_text )
{
	GtkWidget *text_area_w;

	/* Text (area) widget */
	text_area_w = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_area_w), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_area_w), GTK_WRAP_WORD);
	if (init_text != NULL) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_area_w));
		gtk_text_buffer_set_text(buffer, init_text, -1);
	}
	parent_child( parent_w, text_area_w );

	return text_area_w;
}


/* This changes the packing flags of a widget inside a box widget. This
 * allows finer control than gtk_box_set_packing( ) (as this only affects
 * a single widget) */
void
gui_widget_packing( GtkWidget *widget, boolean expand, boolean fill, boolean start )
{
	GtkWidget *parent_box_w;

	parent_box_w = gtk_widget_get_parent( widget );
	g_assert( GTK_IS_BOX(parent_box_w) );

	gtk_box_set_child_packing( GTK_BOX(parent_box_w), widget, expand, fill, 0, start ? GTK_PACK_START : GTK_PACK_END );
}


/* Internal callback for the color selection window, called when the
 * OK button is pressed */
static void
colorsel_window_cb( GtkWidget *colorsel_window_w )
{
	RGBcolor color;
	GdkColor gdk_color;
	GtkWidget *colorsel_w;
	void (*user_callback)( const RGBcolor *, void * );
	void *user_callback_data;

	colorsel_w = gtk_color_selection_dialog_get_color_selection( GTK_COLOR_SELECTION_DIALOG(colorsel_window_w) );
	gtk_color_selection_get_current_color( GTK_COLOR_SELECTION(colorsel_w), &gdk_color );
	color.r = (float)gdk_color.red / 65535.0;
	color.g = (float)gdk_color.green / 65535.0;
	color.b = (float)gdk_color.blue / 65535.0;

	user_callback = (void (*)( const RGBcolor *, void * ))g_object_get_data( G_OBJECT(colorsel_window_w), "user_callback" );
	user_callback_data = g_object_get_data( G_OBJECT(colorsel_window_w), "user_callback_data" );
	gtk_widget_destroy( colorsel_window_w );

	/* Call user callback */
	(user_callback)( &color, user_callback_data );
}


/* Creates a color selection window. OK button activates ok_callback */
GtkWidget *
gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *colorsel_window_w;
	GtkWidget *colorsel_w;
	GtkWidget *ok_button_w;
	GtkWidget *cancel_button_w;
	GdkColor gdk_color;

	colorsel_window_w = gtk_color_selection_dialog_new( title );
	colorsel_w = gtk_color_selection_dialog_get_color_selection( GTK_COLOR_SELECTION_DIALOG(colorsel_window_w) );
	gdk_color.red = init_color->r * 65535;
	gdk_color.green = init_color->g * 65535;
	gdk_color.blue = init_color->b * 65535;
	gtk_color_selection_set_current_color( GTK_COLOR_SELECTION(colorsel_w), &gdk_color );
	g_object_get( colorsel_window_w, "ok-button", &ok_button_w, "cancel-button", &cancel_button_w, NULL );
	g_object_set_data( G_OBJECT(colorsel_window_w), "user_callback", (void *)ok_callback );
	g_object_set_data( G_OBJECT(colorsel_window_w), "user_callback_data", ok_callback_data );
	g_signal_connect_swapped( G_OBJECT(ok_button_w), "clicked", G_CALLBACK(colorsel_window_cb), colorsel_window_w );
	g_signal_connect_swapped( G_OBJECT(cancel_button_w), "clicked", G_CALLBACK(gtk_widget_destroy), colorsel_window_w );
	gtk_widget_show( colorsel_window_w );

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(colorsel_window_w), TRUE );

	return colorsel_window_w;
}


/* Creates a base dialog window. close_callback is called when the
 * window is destroyed */
GtkWidget *
gui_dialog_window( const char *title, void (*close_callback)( ) )
{
	GtkWidget *window_w;

	window_w = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_resizable( GTK_WINDOW(window_w), FALSE );
	gtk_window_set_position( GTK_WINDOW(window_w), GTK_WIN_POS_CENTER );
	gtk_window_set_title( GTK_WINDOW(window_w), title );
	g_signal_connect( G_OBJECT(window_w), "delete_event", G_CALLBACK(gtk_widget_destroy), NULL );
	if (close_callback != NULL)
		g_signal_connect( G_OBJECT(window_w), "destroy", G_CALLBACK(close_callback), NULL );
	/* !gtk_widget_show( ) */

	return window_w;
}


/* Internal callback for the text-entry window, called when the
 * OK button is pressed */
static void
entry_window_cb( GtkWidget *unused, GtkWidget *entry_window_w )
{
	GtkWidget *entry_w;
	char *entry_text;
	void (*user_callback)( const char *, void * );
	void *user_callback_data;

	entry_w = g_object_get_data( G_OBJECT(entry_window_w), "entry_w" );
	entry_text = xstrdup( gtk_entry_get_text( GTK_ENTRY(entry_w) ) );

	user_callback = (void (*)( const char *, void * ))g_object_get_data( G_OBJECT(entry_window_w), "user_callback" );
	user_callback_data = g_object_get_data( G_OBJECT(entry_window_w), "user_callback_data" );
	gtk_widget_destroy( entry_window_w );

	/* Call user callback */
	(user_callback)( entry_text, user_callback_data );
        xfree( entry_text );
}


/* Creates a one-line text-entry window, initialized with the given text
 * string. OK button activates ok_callback */
GtkWidget *
gui_entry_window( const char *title, const char *init_text, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *entry_window_w;
	GtkWidget *frame_w;
	GtkWidget *vbox_w;
	GtkWidget *entry_w;
	GtkWidget *hbox_w;
	GtkWidget *button_w;
        int width;

	entry_window_w = gui_dialog_window( title, NULL );
	gtk_container_set_border_width( GTK_CONTAINER(entry_window_w), 5 );
	width = gdk_screen_get_width( gdk_screen_get_default( ) ) / 2;
	gtk_widget_set_size_request( entry_window_w, width, -1 );
	g_object_set_data( G_OBJECT(entry_window_w), "user_callback", (void *)ok_callback );
	g_object_set_data( G_OBJECT(entry_window_w), "user_callback_data", ok_callback_data );

	frame_w = gui_frame_add( entry_window_w, NULL );
	vbox_w = gui_vbox_add( frame_w, 10 );

        /* Text entry widget */
	entry_w = gui_entry_add( vbox_w, init_text, entry_window_cb, entry_window_w );
	g_object_set_data( G_OBJECT(entry_window_w), "entry_w", entry_w );

	/* Horizontal box for buttons */
	hbox_w = gui_hbox_add( vbox_w, 0 );
	gtk_box_set_homogeneous( GTK_BOX(hbox_w), TRUE );
	gui_box_set_packing( hbox_w, EXPAND, FILL, AT_START );

	/* OK/Cancel buttons */
	gui_button_add( hbox_w, _("OK"), entry_window_cb, entry_window_w );
	vbox_w = gui_vbox_add( hbox_w, 0 ); /* spacer */
	button_w = gui_button_add( hbox_w, _("Cancel"), NULL, NULL );
	g_signal_connect_swapped( G_OBJECT(button_w), "clicked", G_CALLBACK(gtk_widget_destroy), G_OBJECT(entry_window_w) );

	gtk_widget_show( entry_window_w );
	gtk_widget_grab_focus( entry_w );

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(entry_window_w), TRUE );

	return entry_window_w;
}


/* Internal callback for the file chooser dialog response */
static void
filesel_window_response_cb( GtkDialog *dialog, gint response_id, gpointer data )
{
	char *filename;
	void (*user_callback)( const char *, void * );
	void *user_callback_data;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
		user_callback = (void (*)( const char *, void * ))g_object_get_data( G_OBJECT(dialog), "user_callback" );
		user_callback_data = g_object_get_data( G_OBJECT(dialog), "user_callback_data" );
		gtk_widget_destroy( GTK_WIDGET(dialog) );

		/* Call user callback */
		(user_callback)( filename, user_callback_data );

		g_free( filename );
	}
	else {
		gtk_widget_destroy( GTK_WIDGET(dialog) );
	}
}


/* Creates a file chooser window, with an optional default filename.
 * OK button activates ok_callback */
GtkWidget *
gui_filesel_window( const char *title, const char *init_filename, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *filesel_window_w;

	filesel_window_w = gtk_file_chooser_dialog_new( title, NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		NULL );
	if (init_filename != NULL)
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(filesel_window_w), init_filename );
	gtk_window_set_position( GTK_WINDOW(filesel_window_w), GTK_WIN_POS_CENTER );
	g_object_set_data( G_OBJECT(filesel_window_w), "user_callback", (void *)ok_callback );
	g_object_set_data( G_OBJECT(filesel_window_w), "user_callback_data", ok_callback_data );
	g_signal_connect( G_OBJECT(filesel_window_w), "response", G_CALLBACK(filesel_window_response_cb), NULL );
	/* no gtk_widget_show( ) */

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(filesel_window_w), TRUE );

	return filesel_window_w;
}


/* Associates an icon (created from XPM data) to a window */
void
gui_window_icon_xpm( GtkWidget *window_w, char **xpm_data )
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)xpm_data );
	gtk_window_set_icon( GTK_WINDOW(window_w), pixbuf );
	g_object_unref( pixbuf );
}


/* Helper function for gui_window_modalize( ), called upon the destruction
 * of the modal window */
static void
window_unmodalize( GObject *unused, GtkWidget *parent_window_w )
{
	gtk_widget_set_sensitive( parent_window_w, TRUE );
	gui_cursor( parent_window_w, -1 );
}


/* Makes a window modal w.r.t its parent window */
void
gui_window_modalize( GtkWidget *window_w, GtkWidget *parent_window_w )
{
	gtk_window_set_transient_for( GTK_WINDOW(window_w), GTK_WINDOW(parent_window_w) );
	gtk_window_set_modal( GTK_WINDOW(window_w), TRUE );
	gtk_widget_set_sensitive( parent_window_w, FALSE );
	gui_cursor( parent_window_w, GDK_X_CURSOR );

	/* Restore original state once the window is destroyed */
	g_signal_connect( G_OBJECT(window_w), "destroy", G_CALLBACK(window_unmodalize), parent_window_w );
}


#if 0
/* The following is stuff that isn't being used right now (obviously),
 * but may be in the future. TODO: Delete this section by v1.0! */


/* The check button widget */
GtkWidget *
gui_check_button_add( GtkWidget *parent_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *cbutton_w;

	cbutton_w = gtk_check_button_new_with_label( label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cbutton_w), init_state );
	gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(cbutton_w), TRUE );
	if (callback != NULL)
		g_signal_connect( G_OBJECT(cbutton_w), "toggled", G_CALLBACK(callback), callback_data );
	parent_child( parent_w, cbutton_w );

	return cbutton_w;
}


/* Adds a check menu item to a menu */
GtkWidget *
gui_check_menu_item_add( GtkWidget *menu_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *chkmenu_item_w;

	chkmenu_item_w = gtk_check_menu_item_new_with_label( label );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(chkmenu_item_w), init_state );
	gtk_check_menu_item_set_show_toggle( GTK_CHECK_MENU_ITEM(chkmenu_item_w), TRUE );
	gtk_menu_shell_append( GTK_MENU_SHELL(menu_w), chkmenu_item_w );
	g_signal_connect( G_OBJECT(chkmenu_item_w), "toggled", G_CALLBACK(callback), callback_data );
	gtk_widget_show( chkmenu_item_w );

	return chkmenu_item_w;
}


/* Resizes an entry to fit the width of the specified string */
void
gui_entry_set_width( GtkWidget *entry_w, const char *str )
{
	PangoLayout *layout;
	int width;

	layout = gtk_widget_create_pango_layout( entry_w, str );
	pango_layout_get_pixel_size( layout, &width, NULL );
	g_object_unref( layout );
	gtk_widget_set_size_request( entry_w, width + 16, -1 );
}


/* The spin button widget */
GtkWidget *
gui_spin_button_add( GtkWidget *parent_w, GtkAdjustment *adj )
{
	GtkWidget *spinbtn_w;

	spinbtn_w = gtk_spin_button_new( adj, 0.0, 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), spinbtn_w, FALSE, FALSE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), spinbtn_w );
	gtk_widget_show( spinbtn_w );

	return spinbtn_w;
}


/* Returns the width of string, when drawn in the given widget */
int
gui_string_width( const char *str, GtkWidget *widget )
{
	PangoLayout *layout;
	int width;

	layout = gtk_widget_create_pango_layout( widget, str );
	pango_layout_get_pixel_size( layout, &width, NULL );
	g_object_unref( layout );
	return width;
}


/* The horizontal value slider widget */
GtkWidget *
gui_hscale_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *hscale_w;

	hscale_w = gtk_hscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_digits( GTK_SCALE(hscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), hscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), hscale_w );
	gtk_widget_show( hscale_w );

	return hscale_w;
}


/* The vertical value slider widget */
GtkWidget *
gui_vscale_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *vscale_w;

	vscale_w = gtk_vscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_value_pos( GTK_SCALE(vscale_w), GTK_POS_RIGHT );
	gtk_scale_set_digits( GTK_SCALE(vscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), vscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), vscale_w );
	gtk_widget_show( vscale_w );

	return vscale_w;
}


/* Associates a tooltip with a widget */
void
gui_tooltip_add( GtkWidget *widget, const char *tip_text )
{
	static GtkTooltips *tooltips = NULL;

	if (tooltips == NULL) {
		tooltips = gtk_tooltips_new( );
		gtk_tooltips_set_delay( tooltips, 2000 );
	}
	gtk_tooltips_set_tip( tooltips, widget, tip_text, NULL );
}
#endif /* 0 */


/* end gui.c */
