/* ogl.h */

/* Primary OpenGL interface */

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


#ifdef FSV_OGL_H
	#error
#endif
#define FSV_OGL_H


void ogl_make_current( void );
void ogl_queue_render( void );
void ogl_resize( void );
void ogl_refresh( void );
double ogl_aspect_ratio( void );
void ogl_draw( void );
unsigned int ogl_color_pick( int x, int y, unsigned int *face_id );
void ogl_pick_invalidate( void );
#ifdef __GTK_H__
GtkWidget *ogl_widget_new( void );
gboolean ogl_gl_query( void );
#endif


/* end ogl.h */
