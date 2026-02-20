/* viewport.c */

/* Viewport routines */

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
#include "viewport.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "dialog.h" /* context_menu( ) */
#include "filelist.h" /* filelist_show_entry( ) */
#include "geometry.h"
#include "gui.h"
#include "ogl.h"
#include "window.h"


/* Sensitivity factor used for manual camera control */
#define MOUSE_SENSITIVITY 0.5


/* The node table, used to find a node by its ID number */
static GNode **node_table = NULL;
static unsigned int node_table_size = 0;

/* The currently highlighted (indicated) node */
static GNode *indicated_node = NULL;

/* Previous mouse pointer coordinates */
static int prev_x = 0, prev_y = 0;

/* Drag detection state */
#define DRAG_THRESHOLD 4
static boolean btn1_pressed = FALSE;
static boolean btn1_is_dragging = FALSE;
static int btn1_press_x = 0, btn1_press_y = 0;


/* Receives a newly created node table from scanfs( ) */
void
viewport_pass_node_table( GNode **new_node_table, unsigned int table_size )
{
	if (node_table != NULL)
		xfree( node_table );

	node_table = new_node_table;
	node_table_size = table_size;
}


/* This returns the node (if any) that is visible at viewport location
 * (x,y) (where (0,0) indicates the upper-left corner). The ID number of
 * the particular face being pointed at is stored in face_id.
 * Uses color-buffer picking: renders scene with node IDs as colors,
 * reads back the pixel to determine the node. */
static GNode *
node_at_location( int x, int y, unsigned int *face_id )
{
	unsigned int node_id;

	node_id = ogl_color_pick( x, y, face_id );
	if (node_id != 0 && node_id < node_table_size)
		return node_table[node_id];

	return NULL;
}


/* This callback catches all events for the viewport */
int
viewport_cb( GtkWidget *gl_area_w, GdkEvent *event )
{
	GdkEventButton *ev_button;
	GdkEventMotion *ev_motion;
	double dx, dy;
	unsigned int face_id;
	int x, y;
	boolean btn1, btn2, btn3;

	/* GtkGLArea handles expose/configure via its own signals */
	if (event->type == GDK_CONFIGURE)
		return FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		/* Exit the About presentation if it is up */
		if (about( ABOUT_END )) {
			indicated_node = NULL;
			return FALSE;
		}
	}

	/* If we're in splash screen mode, proceed no further */
	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	/* Mouse-related events */
	switch (event->type) {
		case GDK_BUTTON_PRESS:
		ev_button = (GdkEventButton *)event;
		btn1 = ev_button->button == 1;
		btn2 = ev_button->button == 2;
		btn3 = ev_button->button == 3;
		x = (int)ev_button->x;
		y = (int)ev_button->y;
		if (camera_moving( )) {
			camera_pan_finish( );
			indicated_node = NULL;
		}
		else if (btn1) {
			/* Record press position for drag detection */
			btn1_pressed = TRUE;
			btn1_is_dragging = FALSE;
			btn1_press_x = x;
			btn1_press_y = y;
			/* Identify and highlight node under cursor */
			indicated_node = node_at_location( x, y, &face_id );
			if (indicated_node == NULL) {
				geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, "" );
			}
			else {
				geometry_highlight_node( indicated_node, TRUE );
				window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
			}
		}
		else if (btn2) {
			indicated_node = NULL;
			geometry_highlight_node( NULL, FALSE );
			window_statusbar( SB_RIGHT, "" );
		}
		else if (btn3) {
			/* Right-click: context menu */
			GNode *menu_node = node_at_location( x, y, &face_id );
			indicated_node = menu_node;
			if (menu_node != NULL) {
				geometry_highlight_node( menu_node, FALSE );
				window_statusbar( SB_RIGHT, node_absname( menu_node ) );
				/* Note: context_menu() may trigger re-entrant events
				 * (e.g. leave-notify from pointer grab) that clear
				 * indicated_node, so use the local copy after this */
				context_menu( menu_node, ev_button );
				filelist_show_entry( menu_node );
			}
		}
		prev_x = x;
		prev_y = y;
		break;

		case GDK_2BUTTON_PRESS:
		/* Double-click: navigate to node */
		ev_button = (GdkEventButton *)event;
		if (ev_button->button == 1 && !camera_moving( ) && indicated_node != NULL) {
			camera_look_at( indicated_node );
			btn1_pressed = FALSE;
			btn1_is_dragging = FALSE;
		}
		break;

		case GDK_BUTTON_RELEASE:
		ev_button = (GdkEventButton *)event;
		if (ev_button->button == 1) {
			btn1_pressed = FALSE;
			btn1_is_dragging = FALSE;
		}
		gui_cursor( gl_area_w, -1 );
		break;

		case GDK_MOTION_NOTIFY:
		ev_motion = (GdkEventMotion *)event;
		btn1 = ev_motion->state & GDK_BUTTON1_MASK;
		btn2 = ev_motion->state & GDK_BUTTON2_MASK;
		x = (int)ev_motion->x;
		y = (int)ev_motion->y;
		if (!camera_moving( )) {
			if (btn2) {
				/* Dolly the camera */
				gui_cursor( gl_area_w, GDK_DOUBLE_ARROW );
				dy = MOUSE_SENSITIVITY * (y - prev_y);
				camera_dolly( - dy );
				indicated_node = NULL;
			}
			else if (btn1) {
				/* Check if drag threshold exceeded */
				if (!btn1_is_dragging) {
					int total_dx = x - btn1_press_x;
					int total_dy = y - btn1_press_y;
					if (abs( total_dx ) > DRAG_THRESHOLD || abs( total_dy ) > DRAG_THRESHOLD) {
						btn1_is_dragging = TRUE;
						indicated_node = NULL;
						geometry_highlight_node( NULL, FALSE );
						window_statusbar( SB_RIGHT, "" );
					}
				}
				if (btn1_is_dragging) {
					/* Orbit the camera */
					gui_cursor( gl_area_w, GDK_FLEUR );
					dx = MOUSE_SENSITIVITY * (x - prev_x);
					dy = MOUSE_SENSITIVITY * (y - prev_y);
					camera_revolve( dx, dy );
				}
			}
			else {
				/* No button: hover highlight */
				indicated_node = node_at_location( x, y, &face_id );
				if (indicated_node == NULL) {
					geometry_highlight_node( NULL, FALSE );
					window_statusbar( SB_RIGHT, "" );
				}
				else {
					if (geometry_should_highlight( indicated_node, face_id ))
						geometry_highlight_node( indicated_node, FALSE );
					else
						geometry_highlight_node( NULL, FALSE );
					window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
				}
			}
			prev_x = x;
			prev_y = y;
		}
		break;

		case GDK_SCROLL:
		/* Scroll wheel zoom */
		if (!camera_moving( )) {
			GdkEventScroll *ev_scroll = (GdkEventScroll *)event;
			double delta = 0.0;
			if (ev_scroll->direction == GDK_SCROLL_UP)
				delta = -1.0;
			else if (ev_scroll->direction == GDK_SCROLL_DOWN)
				delta = 1.0;
			else if (ev_scroll->direction == GDK_SCROLL_SMOOTH)
				delta = ev_scroll->delta_y;
			if (delta != 0.0)
				camera_dolly( delta * 16.0 );
			indicated_node = NULL;
		}
		break;

		case GDK_LEAVE_NOTIFY:
		/* The mouse has left the viewport */
		geometry_highlight_node( NULL, FALSE );
		window_statusbar( SB_RIGHT, "" );
		gui_cursor( gl_area_w, -1 );
		indicated_node = NULL;
		btn1_pressed = FALSE;
		btn1_is_dragging = FALSE;
		break;

		default:
		/* Ignore event */
		break;
	}

	return FALSE;
}


/* end viewport.c */
