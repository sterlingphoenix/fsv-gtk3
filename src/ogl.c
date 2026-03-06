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

/* Private FBO for color picking (keeps pick renders off the display FBO) */
static GLuint pick_fbo = 0;
static GLuint pick_color_rb = 0;
static GLuint pick_depth_rb = 0;
static int pick_fb_width = 0;
static int pick_fb_height = 0;
static boolean pick_fbo_valid = FALSE;


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


/* Ensures the pick FBO exists and matches the viewport size */
static void
pick_fbo_ensure( int width, int height )
{
	if (pick_fbo != 0 && pick_fb_width == width && pick_fb_height == height)
		return;

	if (pick_fbo == 0) {
		glGenFramebuffers( 1, &pick_fbo );
		glGenRenderbuffers( 1, &pick_color_rb );
		glGenRenderbuffers( 1, &pick_depth_rb );
	}

	glBindRenderbuffer( GL_RENDERBUFFER, pick_color_rb );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, width, height );
	glBindRenderbuffer( GL_RENDERBUFFER, pick_depth_rb );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	glBindFramebuffer( GL_FRAMEBUFFER, pick_fbo );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_RENDERBUFFER, pick_color_rb );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER, pick_depth_rb );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	pick_fb_width = width;
	pick_fb_height = height;
	pick_fbo_valid = FALSE;
}


/* Color-buffer picking: renders the scene with node IDs encoded as colors,
 * then reads the pixel at (x,y) to determine which node is there.
 * Uses a private FBO so the display framebuffer is never disturbed.
 * The pick FBO is cached -- re-rendered only when invalidated by camera
 * or scene changes (via ogl_pick_invalidate).
 * Returns the node ID (0 = no hit). face_id is set from the alpha channel. */
unsigned int
ogl_color_pick( int x, int y, unsigned int *face_id )
{
	GLint viewport[4];
	unsigned char pixel[4] = { 0, 0, 0, 0 };
	unsigned int node_id;

	/* Ensure GL context is current */
	ogl_make_current( );

	/* Get viewport dimensions */
	glGetIntegerv( GL_VIEWPORT, viewport );

	/* Set up the pick FBO (may invalidate if resized) */
	pick_fbo_ensure( viewport[2], viewport[3] );

	if (!pick_fbo_valid) {
		/* Re-render the pick scene into the FBO */
		glBindFramebuffer( GL_FRAMEBUFFER, pick_fbo );
		glViewport( 0, 0, viewport[2], viewport[3] );

		/* Set up for flat-color picking (no lighting/texturing) */
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

		/* Restore GtkGLArea's FBO and GL state for normal rendering */
		gtk_gl_area_attach_buffers( GTK_GL_AREA(viewport_gl_area_w) );
		glViewport( viewport[0], viewport[1], viewport[2], viewport[3] );
		glClearColor( 0.0, 0.0, 0.0, 0.0 );
		glEnable( GL_LIGHTING );
		glShadeModel( GL_FLAT );
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_CULL_FACE );
		glEnable( GL_POLYGON_OFFSET_FILL );

		pick_fbo_valid = TRUE;
	}

	/* Read the pixel at (x, y) from the cached pick FBO */
	glBindFramebuffer( GL_READ_FRAMEBUFFER, pick_fbo );
	glReadPixels( x, viewport[3] - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );

	/* Decode node ID from RGB, face ID from alpha */
	node_id = ((unsigned int)pixel[0] << 16) |
	          ((unsigned int)pixel[1] << 8) |
	          (unsigned int)pixel[2];
	*face_id = (unsigned int)pixel[3];

	return node_id;
}


/* Marks the cached pick FBO as stale. Called when camera position,
 * scene geometry, or viewport size changes. */
void
ogl_pick_invalidate( void )
{
	pick_fbo_valid = FALSE;
}


/* GtkGLArea "realize" signal handler */
static void
realize_cb( GtkWidget *widget, G_GNUC_UNUSED gpointer user_data )
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
render_cb( G_GNUC_UNUSED GtkGLArea *area, G_GNUC_UNUSED GdkGLContext *context, G_GNUC_UNUSED gpointer user_data )
{
	ogl_draw( );
	return TRUE;
}


/* GtkGLArea "resize" signal handler */
static void
resize_cb( G_GNUC_UNUSED GtkGLArea *area, gint width, gint height, G_GNUC_UNUSED gpointer user_data )
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
