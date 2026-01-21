/* gui.c */

/* Higher-level GTK+ interface */

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
#include <gtk/gtk.h>
#include "gui.h"
#ifndef NIL
#define NIL 0
#endif

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

	hbox_w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing );
	gtk_container_set_border_width( GTK_CONTAINER(hbox_w), spacing );
	parent_child( parent_w, hbox_w );

	return hbox_w;
}


/* The vertical box widget */
GtkWidget *
gui_vbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *vbox_w;

	vbox_w = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing );
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


/* The [multi-column] list widget (fitted into a scrolled window) */
GtkWidget *
gui_clist_add( GtkWidget *parent_w, int num_cols, char *col_titles[] )
{
	GtkWidget *scrollwin_w;
	GtkWidget *treeview_w;
    GtkListStore *liststore;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    int i;

	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

    GType* types = g_new(GType, num_cols);
    for(i=0; i<num_cols; i++) types[i] = G_TYPE_STRING;
	liststore = gtk_list_store_newv( num_cols, types );
    g_free(types);

	treeview_w = gtk_tree_view_new_with_model( GTK_TREE_MODEL(liststore) );
    g_object_unref(liststore);

	for (i = 0; i < num_cols; i++) {
		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(col_titles ? col_titles[i] : "", renderer, "text", i, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_w), column);
	}
	
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_w)), GTK_SELECTION_SINGLE);
	gtk_container_add( GTK_CONTAINER(scrollwin_w), treeview_w );
	gtk_widget_show( treeview_w );

	return treeview_w;
}


/* Scrolls a clist (or ctree) to a given row (-1 indicates last row)
 * WARNING: This implementation does not gracefully handle multiple
 * animated scrolls on the same clist! */
void
gui_clist_moveto_row( GtkWidget *clist_w, int row, double moveto_time )
{
	GtkAdjustment *clist_vadj;
	double *anim_value_var;
	float k, new_value;
	int i;

	if (moveto_time <= 0.0) {
		/* Instant scroll (no animation) */
		if (row >= 0)
			i = row;
		else
			i = -1; /* TODO */ /* bottom */
		/* gtk_clist_moveto */( clist_w, i, 0, 0.5, 0.0 );
		return;
	}

	if (row >= 0)
		k=0; /* TODO */
	else
		k = 1.0; /* bottom of clist */
	clist_vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(clist_w));
	k = k * gtk_adjustment_get_upper(clist_vadj) - 0.5 * gtk_adjustment_get_page_size(clist_vadj);
	new_value = CLAMP(k, 0.0, gtk_adjustment_get_upper(clist_vadj) - gtk_adjustment_get_page_size(clist_vadj));

	/* Allocate an external value variable if clist adjustment doesn't
	 * already have one */
        anim_value_var = g_object_get_data( G_OBJECT(clist_vadj), "anim_value_var" );
	if (anim_value_var == NULL ); {
		anim_value_var = NEW(double);
		g_object_set_data_full( G_OBJECT(clist_vadj), "anim_value_var", anim_value_var, _xfree );
	}

	/* If clist is already scrolling, stop it */
	morph_break( anim_value_var );

	/* Begin clist animation */
	*anim_value_var = gtk_adjustment_get_value(clist_vadj);
	morph_full( anim_value_var, MORPH_SIGMOID, new_value, moveto_time, adjustment_step_cb, adjustment_step_cb, clist_vadj );
}


/* Internal callback for the color picker widget */
static void
color_picker_cb( GtkWidget *colorpicker_w, unsigned int r, unsigned int g, unsigned int b, unsigned int unused, void *data )
{
	void (*user_callback)( RGBcolor *, void * );
	RGBcolor color;

	color.r = (float)r / 65535.0;
	color.g = (float)g / 65535.0;
	color.b = (float)b / 65535.0;

	/* Call user callback */
	user_callback = (void (*)( RGBcolor *, void * ))g_object_get_data( G_OBJECT(colorpicker_w), "user_callback" );
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
	GdkRGBA gdk_color = {
		.red = color->r,
		.green = color->g,
		.blue = color->b, .alpha = 1.0
	};

	gtk_color_button_set_rgba(GTK_COLOR_BUTTON(colorbutton_w), &gdk_color);
}


/* The tree widget (fitted into a scrolled window) */
GtkWidget *
gui_ctree_add( GtkWidget *parent_w )
{
	GtkWidget *scrollwin_w;
	GtkWidget *treeview_w;
    GtkTreeStore *treestore;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	treestore = gtk_tree_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER );
	treeview_w = gtk_tree_view_new_with_model( GTK_TREE_MODEL(treestore) );
    g_object_unref(treestore);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "Tree", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_w), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_w), FALSE);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_w)), GTK_SELECTION_BROWSE );
	gtk_container_add( GTK_CONTAINER(scrollwin_w), treeview_w );
	gtk_widget_show( treeview_w );

	return treeview_w;
}


/* This adds a new GtkTreeIter (tree item) to the given ctree.
 * GtkWidget *ctree_w: the ctree widget
 * GtkTreeIter *parent: the parent node (NULL if creating a top-level node)
 * Icon icon_pair[2]: two icons, for collapsed ([0]) and expanded ([1]) states
 * const char *text: label for node
 * boolean expanded: initial state of node
 * void *data: arbitrary pointer to associate data with node */
GtkTreeIter *
gui_ctree_node_add( GtkWidget *treeview_w, GtkTreeIter *parent, Icon icon_pair[2], const char *text, boolean expanded, void *data )
{
    GtkTreeStore *treestore = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_w)));
	GtkTreeIter *iter = g_new(GtkTreeIter, 1);

    gtk_tree_store_append(treestore, iter, parent);
    gtk_tree_store_set(treestore, iter, 0, text, 1, data, -1);

    if (expanded) {
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), iter);
        if (path) {
            gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview_w), path, FALSE);
            gtk_tree_path_free(path);
        }
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
		cursor = gdk_cursor_new_for_display(gdk_display_get_default(), (GdkCursorType)glyph);
	else
		cursor = NULL;
	gdk_window_set_cursor( gtk_widget_get_window(widget), cursor );

	/* Don't need the old cursor anymore */
	if (prev_cursor != NULL)
		g_object_unref( prev_cursor );

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

	/*dateedit_w = gnome_date_edit_new( the_time, TRUE, TRUE );
	gnome_date_edit_set_popup_range( GNOME_DATE_EDIT(dateedit_w), 0, 23 );
	g_signal_connect( G_OBJECT(dateedit_w), "date_changed", G_CALLBACK(callback), callback_data );
	g_signal_connect( G_OBJECT(dateedit_w), "time_changed", G_CALLBACK(callback), callback_data );
	parent_child( parent_w, dateedit_w );*/

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
	return gtk_entry_get_text( GTK_ENTRY(entry_w) );
}


/* Highlights the text in an entry */
void
gui_entry_highlight( GtkWidget *entry_w )
{
	gtk_editable_select_region( GTK_EDITABLE(entry_w), 0, gtk_entry_get_text_length(GTK_ENTRY(entry_w)) );
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
		gtk_widget_add_accelerator( widget, "activate", accel_group, key, mods, 0 );
		return;
	}
	if (GTK_IS_BUTTON(widget)) {
		gtk_widget_add_accelerator( widget, "clicked", accel_group, key, mods, 0 );
		return;
	}

	/* Make widget grab focus when its key is pressed */
	gtk_widget_add_accelerator( widget, "grab_focus", accel_group, key, mods, 0 );
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
	/* parent_menu_item is not accessible anymore */

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


/* The option menu widget. Options must have already been defined using
 * gui_option_menu_item( ) */
GtkWidget *
gui_option_menu_add( GtkWidget *parent_w, int init_selected )
{
    static GtkWidget *combo_box = NULL;
    static GSList *callbacks = NULL;

    if (GTK_IS_MENU_ITEM(parent_w)) {
        if (combo_box == NULL) {
            combo_box = gtk_combo_box_text_new();
        }
        const char *label = gtk_menu_item_get_label(GTK_MENU_ITEM(parent_w));
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), label);
        
        void *callback = g_object_get_data(G_OBJECT(parent_w), "callback");
        void *callback_data = g_object_get_data(G_OBJECT(parent_w), "callback_data");
        
        callbacks = g_slist_append(callbacks, callback);
        callbacks = g_slist_append(callbacks, callback_data);
        
    } else {
        if (combo_box) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), init_selected);
            parent_child(parent_w, combo_box);
            
            // Connect signals
            void changed_cb(GtkComboBox *widget, gpointer user_data) {
                int active = gtk_combo_box_get_active(widget);
                GSList *cb_list = (GSList*)user_data;
                void (*callback)(void) = g_slist_nth_data(cb_list, active * 2);
                void* callback_data = g_slist_nth_data(cb_list, active * 2 + 1);
                if (callback) {
                    callback();
                }
            }
            g_signal_connect(combo_box, "changed", G_CALLBACK(changed_cb), callbacks);
        }
        GtkWidget *ret_combo_box = combo_box;
        combo_box = NULL;
        callbacks = NULL; // Reset for next option menu
        return ret_combo_box;
    }
    return combo_box;
}


/* Option menu definiton. Call this once for each menu item, and then call
 * gui_option_menu_add( ) to produce the finished widget */
GtkWidget *
gui_option_menu_item( const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *menu_item_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	if (callback != NULL) {
        g_object_set_data(G_OBJECT(menu_item_w), "callback", callback);
        g_object_set_data(G_OBJECT(menu_item_w), "callback_data", callback_data);
    }
	gui_option_menu_add( menu_item_w, 0 );

	return menu_item_w;
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


/* The pixmap widget (created from XPM data) */
GtkWidget *
gui_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data )
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpm_data);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    parent_child(parent_w, image);
    return image;
}


/* The color preview widget */
GtkWidget *
gui_preview_add( GtkWidget *parent_w )
{
	GtkWidget *preview_w;

	preview_w = gtk_drawing_area_new();
	parent_child_full( parent_w, preview_w, EXPAND, FILL );

	return preview_w;
}


/* Helper callback for gui_preview_spectrum( ) */
/* BUG: This does not handle resizes correctly */
static gboolean
preview_spectrum_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    RGBcolor (*spectrum_func)( double x );
	RGBcolor color;
    int width, height;
    int i;

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

	spectrum_func = (RGBcolor (*)( double x ))g_object_get_data( G_OBJECT(widget), "spectrum_func" );
    if (!spectrum_func)
        return FALSE;

    for (i = 0; i < width; i++) {
        color = (spectrum_func)((double)i / (double)(width - 1));
        cairo_set_source_rgb(cr, color.r, color.g, color.b);
        cairo_rectangle(cr, i, 0, 1, height);
        cairo_fill(cr);
    }

    return TRUE;
}


/* Fills a preview widget with an arbitrary spectrum. Second argument
 * should be a function returning the appropriate color at a specified
 * fractional position in the spectrum */
void
gui_preview_spectrum( GtkWidget *drawing_area, RGBcolor (*spectrum_func)( double x ) )
{
    g_object_set_data(G_OBJECT(drawing_area), "spectrum_func", spectrum_func);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(preview_spectrum_draw_cb), NULL);
    gtk_widget_queue_draw(drawing_area);
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

	hscrollbar_w = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
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

	vscrollbar_w = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
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
			gtk_menu_shell_append( GTK_MENU(parent_w), separator_w );
		}
		else {
			separator_w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
			gtk_box_pack_start( GTK_BOX(parent_w), separator_w, FALSE, FALSE, 10 );
		}
		gtk_widget_show( separator_w );
	}
	else
		separator_w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

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
	static guint context_id = 0;
    if (context_id == 0)
        context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar_w), "status");
	
	gtk_statusbar_pop(GTK_STATUSBAR(statusbar_w), context_id);
    char strbuf[1024];
    snprintf( strbuf, sizeof(strbuf), " %s", message );
	gtk_statusbar_push(GTK_STATUSBAR(statusbar_w), context_id, strbuf);
}


/* The table (layout) widget */
GtkWidget *
gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding )
{
	GtkWidget *table_w;
	int *cp;

	table_w = gtk_grid_new();
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
	gtk_grid_attach(GTK_GRID(table_w), widget, left, top, right-left, bottom-top);
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

	parent_box_w = gtk_widget_get_parent(widget);
	g_assert( GTK_IS_BOX(parent_box_w) );

	gtk_box_set_child_packing( GTK_BOX(parent_box_w), widget, expand, fill, 0, start ? GTK_PACK_START : GTK_PACK_END );
}


/* Internal callback for the color selection window, called when the
 * OK button is pressed */
static void
colorsel_window_cb(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_OK) {
        GdkRGBA color;
        RGBcolor fsv_color;
        void (*user_callback)(const RGBcolor *, void *);
        void *user_callback_data;

        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &color);
        fsv_color.r = color.red;
        fsv_color.g = color.green;
        fsv_color.b = color.blue;

        user_callback = g_object_get_data(G_OBJECT(dialog), "user_callback");
        user_callback_data = g_object_get_data(G_OBJECT(dialog), "user_callback_data");
        if (user_callback) {
            user_callback(&fsv_color, user_callback_data);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


/* Creates a color selection window. OK button activates ok_callback */
GtkWidget *
gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *colorsel_window_w;
	GdkRGBA color_rgba;

	colorsel_window_w = gtk_color_chooser_dialog_new( title, NULL );
	
	color_rgba.red = init_color->r;
	color_rgba.green = init_color->g;
	color_rgba.blue = init_color->b;
	color_rgba.alpha = 1.0;
	gtk_color_chooser_set_rgba( GTK_COLOR_CHOOSER(colorsel_window_w), &color_rgba );

	g_object_set_data( G_OBJECT(colorsel_window_w), "user_callback", (void *)ok_callback );
	g_object_set_data( G_OBJECT(colorsel_window_w), "user_callback_data", ok_callback_data );
	g_signal_connect( colorsel_window_w, "response", G_CALLBACK(colorsel_window_cb), NULL );
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
	gtk_window_set_resizable(GTK_WINDOW(window_w), FALSE);
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
	width = gdk_screen_width( ) / 2;
	gtk_widget_set_size_request( entry_window_w, width, 0 );
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


/* Internal callback for the file selection window, called when the
 * OK button is pressed */
static void
filesel_window_cb(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    if (response_id == GTK_RESPONSE_OK) {
        char *filename;
        void (*user_callback)(const char *, void *);
        void *user_callback_data;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        user_callback = g_object_get_data(G_OBJECT(dialog), "user_callback");
        user_callback_data = g_object_get_data(G_OBJECT(dialog), "user_callback_data");

        if (user_callback) {
            user_callback(filename, user_callback_data);
        }
        g_free(filename);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


/* Creates a file selection window, with an optional default filename.
 * OK button activates ok_callback */
GtkWidget *
gui_filesel_window( const char *title, const char *init_filename, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *filesel_window_w;

	filesel_window_w = gtk_file_chooser_dialog_new (title, NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
	if (init_filename != NULL)
		gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(filesel_window_w), init_filename );
	gtk_window_set_position( GTK_WINDOW(filesel_window_w), GTK_WIN_POS_CENTER );
	g_object_set_data( G_OBJECT(filesel_window_w), "user_callback", (void *)ok_callback );
	g_object_set_data( G_OBJECT(filesel_window_w), "user_callback_data", ok_callback_data );
	g_signal_connect(filesel_window_w, "response", G_CALLBACK(filesel_window_cb), NULL);
	
	
        /* no gtk_widget_show( ) */

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(filesel_window_w), TRUE );

	return filesel_window_w;
}


/* Associates an icon (created from XPM data) to a window */
void
gui_window_icon_xpm( GtkWidget *window_w, char **xpm_data )
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpm_data);
    if (pixbuf) {
        gtk_window_set_icon(GTK_WINDOW(window_w), pixbuf);
        g_object_unref(pixbuf);
    }
}


/* Helper function for gui_window_modalize( ), called upon the destruction
 * of the modal window */
static void
window_unmodalize( GtkWidget *widget, GdkEvent *event, gpointer parent_window_w )
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
	GtkStyle *style;
	int width;

	style = gtk_widget_get_style( entry_w );
	width = gdk_string_width( style->font, str );
	gtk_widget_set_size_request( entry_w, width + 16, 0 );
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
	GtkStyle *style;
	style = gtk_widget_get_style( widget );
	return gdk_string_width( style->font, str );
}


/* The horizontal value slider widget */
GtkWidget *
gui_hscale_add( GtkWidget *parent_w, GObject *adjustment )
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
gui_vscale_add( GtkWidget *parent_w, GObject *adjustment )
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
