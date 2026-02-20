/* ogl.c */

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


#include "common.h"
#include "ogl.h"

#include <gtk/gtk.h>
#include <epoxy/gl.h>

#include "animation.h" /* redraw( ) */
#include "camera.h"
#include "geometry.h"
#include "tmaptext.h" /* text_init( ) */


/* Main viewport OpenGL area widget */
static GtkWidget *viewport_gl_area_w = NULL;


/* Ensures the GL context is current (public interface) */
void
ogl_make_current( void )
{
	if (viewport_gl_area_w != NULL)
		gtk_gl_area_make_current( GTK_GL_AREA(viewport_gl_area_w) );
}


/* Queues a render of the GL viewport */
void
ogl_queue_render( void )
{
	if (viewport_gl_area_w != NULL)
		gtk_gl_area_queue_render( GTK_GL_AREA(viewport_gl_area_w) );
}


/* Initializes OpenGL state */
static void
ogl_init( void )
{
	float light_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
	float light_diffuse[] = { 0.5, 0.5, 0.5, 1.0 };
	float light_specular[] = { 0.4, 0.4, 0.4, 1.0 };
	float light_position[] = { 0.0, 0.0, 0.0, 1.0 };

	/* Set viewport size */
	ogl_resize( );

	/* Create the initial modelview matrix
	 * (right-handed coordinate system, +z = straight up,
	 * camera at origin looking in -x direction) */
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glRotated( -90.0, 1.0, 0.0, 0.0 );
	glRotated( -90.0, 0.0, 0.0, 1.0 );
	glPushMatrix( ); /* Matrix will stay just below top of MVM stack */

	/* Set up lighting */
	glEnable( GL_LIGHTING );
	glEnable( GL_LIGHT0 );
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );

	/* Set up materials */
	glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
	glEnable( GL_COLOR_MATERIAL );

	/* Miscellaneous */
	glAlphaFunc( GL_GEQUAL, 0.0625 );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_CULL_FACE );
	glShadeModel( GL_FLAT );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 1.0, 1.0 );
	glClearColor( 0.0, 0.0, 0.0, 0.0 );

	/* Initialize texture-mapped text engine */
	text_init( );
}


/* Changes viewport size, after a window resize */
void
ogl_resize( void )
{
	GtkAllocation allocation;
	int width, height;

	gtk_widget_get_allocation( viewport_gl_area_w, &allocation );
	width = allocation.width;
	height = allocation.height;
	glViewport( 0, 0, width, height );
}


/* Refreshes viewport after a window unhide, etc. */
void
ogl_refresh( void )
{
	ogl_queue_render( );
	redraw( );
}


/* Returns the viewport's current aspect ratio */
double
ogl_aspect_ratio( void )
{
	GLint viewport[4];

	glGetIntegerv( GL_VIEWPORT, viewport );

	/* aspect_ratio = width / height */
	return (double)viewport[2] / (double)viewport[3];
}


/* Sets up the projection matrix. full_reset should be TRUE unless the
 * current matrix is to be multiplied in */
static void
setup_projection_matrix( boolean full_reset )
{
	double dx, dy;

	dx = camera->near_clip * tan( 0.5 * RAD(camera->fov) );
	dy = dx / ogl_aspect_ratio( );
	glMatrixMode( GL_PROJECTION );
	if (full_reset)
		glLoadIdentity( );
	glFrustum( - dx, dx, - dy, dy, camera->near_clip, camera->far_clip );
}


/* Sets up the modelview matrix */
static void
setup_modelview_matrix( void )
{
	glMatrixMode( GL_MODELVIEW );
	/* Remember, base matrix lives just below top of stack */
	glPopMatrix( );
	glPushMatrix( );

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		break;

		case FSV_DISCV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( 90.0, 0.0, 1.0, 0.0 );
		glRotated( 90.0, 0.0, 0.0, 1.0 );
		glTranslated( - DISCV_CAMERA(camera)->target.x, - DISCV_CAMERA(camera)->target.y, 0.0 );
		break;

		case FSV_MAPV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( - MAPV_CAMERA(camera)->target.x, - MAPV_CAMERA(camera)->target.y, - MAPV_CAMERA(camera)->target.z );
		break;

		case FSV_TREEV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( TREEV_CAMERA(camera)->target.r, 0.0, - TREEV_CAMERA(camera)->target.z );
		glRotated( 180.0 - TREEV_CAMERA(camera)->target.theta, 0.0, 0.0, 1.0 );
		break;

		SWITCH_FAIL
	}
}


/* (Re)draws the viewport
 * NOTE: Don't call this directly! Use redraw( ) */
void
ogl_draw( void )
{
	static FsvMode prev_mode = FSV_NONE;
	int err;

	geometry_highlight_node( NULL, TRUE );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	setup_projection_matrix( TRUE );
	setup_modelview_matrix( );
	geometry_draw( TRUE );

	/* Error check */
	err = glGetError( );
	if (err != 0)
		g_warning( "GL error: 0x%X", err );

	/* First frame after a mode switch is not drawn
	 * (with the exception of splash screen mode) */
	if (globals.fsv_mode != prev_mode) {
		prev_mode = globals.fsv_mode;
                if (globals.fsv_mode != FSV_SPLASH)
			return;
	}
}


/* Color-buffer picking: renders the scene with node IDs encoded as colors,
 * then reads the pixel at (x,y) to determine which node is there.
 * Returns the node ID (0 = no hit). face_id is set from the alpha channel. */
unsigned int
ogl_color_pick( int x, int y, unsigned int *face_id )
{
	GLint viewport[4];
	unsigned char pixel[4] = { 0, 0, 0, 0 };
	unsigned int node_id;
	float save_clear_color[4];

	/* Ensure GL context and FBO are current */
	ogl_make_current( );
	gtk_gl_area_attach_buffers( GTK_GL_AREA(viewport_gl_area_w) );

	/* Save GL state */
	glGetFloatv( GL_COLOR_CLEAR_VALUE, save_clear_color );
	GLboolean save_lighting = glIsEnabled( GL_LIGHTING );
	GLboolean save_texture = glIsEnabled( GL_TEXTURE_2D );
	GLboolean save_blend = glIsEnabled( GL_BLEND );
	GLboolean save_dither = glIsEnabled( GL_DITHER );
	GLboolean save_fog = glIsEnabled( GL_FOG );
	GLboolean save_alpha_test = glIsEnabled( GL_ALPHA_TEST );

	/* Set up for color picking */
	glDisable( GL_LIGHTING );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
	glDisable( GL_DITHER );
	glDisable( GL_FOG );
	glDisable( GL_ALPHA_TEST );
	glShadeModel( GL_FLAT );

	/* Clear to black (node ID 0 = no hit) */
	glClearColor( 0.0, 0.0, 0.0, 0.0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	/* Set up matrices and draw in pick mode */
	setup_projection_matrix( TRUE );
	setup_modelview_matrix( );
	geometry_draw_for_pick( );

	/* Read the pixel at (x, y) */
	glGetIntegerv( GL_VIEWPORT, viewport );
	glReadPixels( x, viewport[3] - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel );

	/* Decode node ID from RGB, face ID from alpha */
	node_id = ((unsigned int)pixel[0] << 16) |
	          ((unsigned int)pixel[1] << 8) |
	          (unsigned int)pixel[2];
	*face_id = (unsigned int)pixel[3];

	/* Restore GL state */
	glClearColor( save_clear_color[0], save_clear_color[1],
	              save_clear_color[2], save_clear_color[3] );
	if (save_lighting) glEnable( GL_LIGHTING );
	if (save_texture) glEnable( GL_TEXTURE_2D );
	if (save_blend) glEnable( GL_BLEND );
	if (save_dither) glEnable( GL_DITHER );
	if (save_fog) glEnable( GL_FOG );
	if (save_alpha_test) glEnable( GL_ALPHA_TEST );
	glShadeModel( GL_FLAT );

	/* Re-render the normal scene so the FBO isn't left with
	 * pick colors (which would briefly flash on screen) */
	ogl_draw( );

	return node_id;
}


/* GtkGLArea "realize" signal handler */
static void
realize_cb( GtkWidget *widget, gpointer user_data )
{
	gtk_gl_area_make_current( GTK_GL_AREA(widget) );
	if (gtk_gl_area_get_error( GTK_GL_AREA(widget) ) != NULL)
		return;

	ogl_init( );

	/* Queue the initial render */
	gtk_gl_area_queue_render( GTK_GL_AREA(widget) );
}


/* GtkGLArea "render" signal handler */
static gboolean
render_cb( GtkGLArea *area, GdkGLContext *context, gpointer user_data )
{
	ogl_draw( );
	return TRUE;
}


/* GtkGLArea "resize" signal handler */
static void
resize_cb( GtkGLArea *area, gint width, gint height, gpointer user_data )
{
	glViewport( 0, 0, width, height );
}


/* Creates the viewport GL widget */
GtkWidget *
ogl_widget_new( void )
{
	viewport_gl_area_w = gtk_gl_area_new( );

	/* Enable depth buffer */
	gtk_gl_area_set_has_depth_buffer( GTK_GL_AREA(viewport_gl_area_w), TRUE );

	/* We control when rendering happens (via queue_render from animation loop) */
	gtk_gl_area_set_auto_render( GTK_GL_AREA(viewport_gl_area_w), FALSE );

	/* Connect signals.
	 * Note: compatibility profile is requested via GDK_GL=legacy
	 * environment variable set in main() before gtk_init() */
	g_signal_connect( viewport_gl_area_w, "realize", G_CALLBACK(realize_cb), NULL );
	g_signal_connect( viewport_gl_area_w, "render", G_CALLBACK(render_cb), NULL );
	g_signal_connect( viewport_gl_area_w, "resize", G_CALLBACK(resize_cb), NULL );

	return viewport_gl_area_w;
}


/* Returns TRUE if GL is available */
gboolean
ogl_gl_query( void )
{
	/* GtkGLArea handles GL capability detection.
	 * Errors are reported via gtk_gl_area_get_error() after realization. */
	return TRUE;
}


/* end ogl.c */
