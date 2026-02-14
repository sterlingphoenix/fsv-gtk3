/* nvstore.c */

/* Nonvolatile storage library */

/* Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "nvstore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>


#define xmalloc	malloc
#define xstrdup	strdup
#define xfree	free


/* Vector iteration state */
typedef struct _VectorState VectorState;
struct _VectorState {
	char *key_prefix;	/* Path prefix for this vector level */
	int counter;		/* Current element index */
};


/* NVStore instance */
struct _NVStore {
	char *filename;		/* Expanded file path */
	char *current_path;	/* Current working path */
	GHashTable *data;	/* Full-path key -> string value */
	GList *vector_stack;	/* Stack of VectorState for iteration */
	int dirty;		/* TRUE if modified */
};


/* Expand ~ to home directory */
static char *
expand_filename( const char *filename )
{
	const char *home;
	char *result;

	if (filename[0] == '~' && filename[1] == '/') {
		home = g_get_home_dir( );
		result = (char *)xmalloc( strlen( home ) + strlen( filename ) );
		strcpy( result, home );
		strcat( result, filename + 1 );
	}
	else {
		result = xstrdup( filename );
	}

	return result;
}


/* Build a full key path from current_path and relative path */
static char *
resolve_path( NVStore *nvs, const char *path )
{
	char *result;
	size_t cp_len;

	if (nvs->current_path == NULL || strlen( nvs->current_path ) == 0) {
		result = (char *)xmalloc( strlen( path ) + 2 );
		sprintf( result, "/%s", path );
	}
	else {
		cp_len = strlen( nvs->current_path );
		result = (char *)xmalloc( cp_len + strlen( path ) + 2 );
		sprintf( result, "%s/%s", nvs->current_path, path );
	}

	return result;
}


/* Parse a key=value config file into the hash table */
static void
nvs_parse_file( NVStore *nvs )
{
	FILE *fp;
	char line[4096];
	char *eq, *key, *value;
	char *k_start, *k_end, *v_start, *v_end;

	fp = fopen( nvs->filename, "r" );
	if (fp == NULL)
		return;

	while (fgets( line, sizeof(line), fp ) != NULL) {
		/* Skip comments and blank lines */
		k_start = line;
		while (*k_start == ' ' || *k_start == '\t')
			k_start++;
		if (*k_start == '#' || *k_start == '\n' || *k_start == '\0')
			continue;

		/* Find = separator */
		eq = strchr( k_start, '=' );
		if (eq == NULL)
			continue;

		/* Extract key (trim whitespace) */
		k_end = eq - 1;
		while (k_end > k_start && (*k_end == ' ' || *k_end == '\t'))
			k_end--;
		*(k_end + 1) = '\0';
		key = xstrdup( k_start );

		/* Extract value (trim whitespace and newline) */
		v_start = eq + 1;
		while (*v_start == ' ' || *v_start == '\t')
			v_start++;
		v_end = v_start + strlen( v_start ) - 1;
		while (v_end > v_start && (*v_end == '\n' || *v_end == '\r' || *v_end == ' ' || *v_end == '\t'))
			v_end--;
		*(v_end + 1) = '\0';
		value = xstrdup( v_start );

		g_hash_table_insert( nvs->data, key, value );
	}

	fclose( fp );
}


/* Comparison function for sorting keys when writing */
static gint
key_compare( gconstpointer a, gconstpointer b )
{
	return strcmp( (const char *)a, (const char *)b );
}


/* Write the hash table to the config file */
static void
nvs_write_file( NVStore *nvs )
{
	FILE *fp;
	GList *keys, *llink;

	fp = fopen( nvs->filename, "w" );
	if (fp == NULL)
		return;

	fprintf( fp, "# fsv configuration file\n" );

	/* Get all keys and sort them */
	keys = NULL;
	{
		GHashTableIter iter;
		gpointer key, value;
		g_hash_table_iter_init( &iter, nvs->data );
		while (g_hash_table_iter_next( &iter, &key, &value )) {
			keys = g_list_prepend( keys, key );
		}
	}
	keys = g_list_sort( keys, key_compare );

	/* Write each key=value pair */
	llink = keys;
	while (llink != NULL) {
		const char *key = (const char *)llink->data;
		const char *value = (const char *)g_hash_table_lookup( nvs->data, key );
		fprintf( fp, "%s = %s\n", key, value );
		llink = llink->next;
	}

	g_list_free( keys );
	fclose( fp );
}


NVStore *
nvs_open( const char *filename )
{
	NVStore *nvs;

	nvs = (NVStore *)xmalloc( sizeof(NVStore) );
	nvs->filename = expand_filename( filename );
	nvs->current_path = xstrdup( "" );
	nvs->data = g_hash_table_new_full( g_str_hash, g_str_equal, xfree, xfree );
	nvs->vector_stack = NULL;
	nvs->dirty = 0;

	/* Parse existing file if present */
	nvs_parse_file( nvs );

	return nvs;
}


NVS_BOOL
nvs_close( NVStore *nvs )
{
	GList *llink;
	VectorState *vs;

	if (nvs == NULL)
		return 0;

	/* Write to disk if modified */
	if (nvs->dirty)
		nvs_write_file( nvs );

	/* Free vector stack */
	llink = nvs->vector_stack;
	while (llink != NULL) {
		vs = (VectorState *)llink->data;
		xfree( vs->key_prefix );
		xfree( vs );
		llink = llink->next;
	}
	g_list_free( nvs->vector_stack );

	g_hash_table_destroy( nvs->data );
	xfree( nvs->current_path );
	xfree( nvs->filename );
	xfree( nvs );

	return 1;
}


void
nvs_change_path( NVStore *nvs, const char *path )
{
	char *new_path;

	if (nvs == NULL)
		return;

	if (strcmp( path, ".." ) == 0) {
		/* Go up one level */
		char *last_slash = strrchr( nvs->current_path, '/' );
		if (last_slash != NULL) {
			*last_slash = '\0';
			new_path = xstrdup( nvs->current_path );
			xfree( nvs->current_path );
			nvs->current_path = new_path;
		}
		return;
	}

	/* Check if we're in a vector context and this path matches
	 * the vector key prefix */
	if (nvs->vector_stack != NULL) {
		VectorState *vs = (VectorState *)nvs->vector_stack->data;
		if (vs->key_prefix == NULL)
			vs->key_prefix = xstrdup( path );
		if (strcmp( path, vs->key_prefix ) == 0) {
			/* Append path with vector index */
			size_t cp_len = strlen( nvs->current_path );
			size_t p_len = strlen( path );
			new_path = (char *)xmalloc( cp_len + p_len + 16 );
			sprintf( new_path, "%s/%s[%d]", nvs->current_path, path, vs->counter );
			vs->counter++;
			xfree( nvs->current_path );
			nvs->current_path = new_path;
			return;
		}
	}

	/* Normal path append */
	new_path = resolve_path( nvs, path );
	xfree( nvs->current_path );
	nvs->current_path = new_path;
}


void
nvs_delete_recursive( NVStore *nvs, const char *path )
{
	char *prefix;
	GList *keys_to_remove, *llink;
	GHashTableIter iter;
	gpointer key, value;

	if (nvs == NULL)
		return;

	if (strcmp( path, "." ) == 0) {
		prefix = xstrdup( nvs->current_path );
	}
	else {
		prefix = resolve_path( nvs, path );
	}

	/* Collect keys that start with prefix */
	keys_to_remove = NULL;
	g_hash_table_iter_init( &iter, nvs->data );
	while (g_hash_table_iter_next( &iter, &key, &value )) {
		if (strncmp( (const char *)key, prefix, strlen( prefix ) ) == 0) {
			keys_to_remove = g_list_prepend( keys_to_remove, xstrdup( (const char *)key ) );
		}
	}

	/* Remove collected keys */
	if (keys_to_remove != NULL)
		nvs->dirty = 1;
	llink = keys_to_remove;
	while (llink != NULL) {
		g_hash_table_remove( nvs->data, (const char *)llink->data );
		xfree( llink->data );
		llink = llink->next;
	}
	g_list_free( keys_to_remove );

	xfree( prefix );
}


void
nvs_vector_begin( NVStore *nvs )
{
	VectorState *vs;

	if (nvs == NULL)
		return;

	vs = (VectorState *)xmalloc( sizeof(VectorState) );
	vs->key_prefix = NULL;
	vs->counter = 0;

	/* The key prefix will be set by the first nvs_path_present or
	 * nvs_change_path call within this vector context */
	nvs->vector_stack = g_list_prepend( nvs->vector_stack, vs );
}


void
nvs_vector_end( NVStore *nvs )
{
	VectorState *vs;

	if (nvs == NULL || nvs->vector_stack == NULL)
		return;

	vs = (VectorState *)nvs->vector_stack->data;
	if (vs->key_prefix != NULL)
		xfree( vs->key_prefix );
	xfree( vs );

	nvs->vector_stack = g_list_delete_link( nvs->vector_stack, nvs->vector_stack );
}


NVS_BOOL
nvs_path_present( NVStore *nvs, const char *path )
{
	char *check_path;
	GHashTableIter iter;
	gpointer key, value;
	size_t check_len;
	NVS_BOOL found = 0;

	if (nvs == NULL)
		return 0;

	/* In vector context, set the key prefix if not yet set,
	 * and use the vector counter for the index */
	if (nvs->vector_stack != NULL) {
		VectorState *vs = (VectorState *)nvs->vector_stack->data;
		if (vs->key_prefix == NULL)
			vs->key_prefix = xstrdup( path );

		/* Check for path[counter] */
		check_path = (char *)xmalloc( strlen( nvs->current_path ) + strlen( path ) + 16 );
		sprintf( check_path, "%s/%s[%d]", nvs->current_path, path, vs->counter );
	}
	else {
		check_path = resolve_path( nvs, path );
	}

	check_len = strlen( check_path );

	/* Check if any key matches or starts with this path */
	g_hash_table_iter_init( &iter, nvs->data );
	while (g_hash_table_iter_next( &iter, &key, &value )) {
		const char *k = (const char *)key;
		if (strncmp( k, check_path, check_len ) == 0) {
			/* Must be exact match or followed by / */
			if (k[check_len] == '\0' || k[check_len] == '/') {
				found = 1;
				break;
			}
		}
	}

	xfree( check_path );
	return found;
}


/* Internal: look up a value by path */
static const char *
nvs_lookup( NVStore *nvs, const char *path )
{
	char *full_path;
	const char *result;

	if (nvs == NULL)
		return NULL;

	full_path = resolve_path( nvs, path );
	result = (const char *)g_hash_table_lookup( nvs->data, full_path );
	xfree( full_path );

	return result;
}


/* Internal: store a value by path */
static void
nvs_store( NVStore *nvs, const char *path, const char *value )
{
	char *full_path;

	if (nvs == NULL)
		return;

	full_path = resolve_path( nvs, path );
	g_hash_table_insert( nvs->data, full_path, xstrdup( value ) );
	nvs->dirty = 1;
}


NVS_BOOL
nvs_read_boolean( NVStore *nvs, const char *path )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return 0;
	return (strcmp( val, "true" ) == 0 || strcmp( val, "1" ) == 0);
}


int
nvs_read_int( NVStore *nvs, const char *path )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return 0;
	return atoi( val );
}


int
nvs_read_int_token( NVStore *nvs, const char *path, const char **tokens )
{
	const char *val = nvs_lookup( nvs, path );
	int i;

	if (val == NULL)
		return 0;

	for (i = 0; tokens[i] != NULL; i++) {
		if (strcmp( val, tokens[i] ) == 0)
			return i;
	}

	return 0;
}


double
nvs_read_float( NVStore *nvs, const char *path )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return 0.0;
	return atof( val );
}


char *
nvs_read_string( NVStore *nvs, const char *path )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return xstrdup( "" );
	return xstrdup( val );
}


NVS_BOOL
nvs_read_boolean_default( NVStore *nvs, const char *path, NVS_BOOL default_val )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return default_val;
	return (strcmp( val, "true" ) == 0 || strcmp( val, "1" ) == 0);
}


int
nvs_read_int_default( NVStore *nvs, const char *path, int default_val )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return default_val;
	return atoi( val );
}


int
nvs_read_int_token_default( NVStore *nvs, const char *path, const char **tokens, int default_val )
{
	const char *val = nvs_lookup( nvs, path );
	int i;

	if (val == NULL)
		return default_val;

	for (i = 0; tokens[i] != NULL; i++) {
		if (strcmp( val, tokens[i] ) == 0)
			return i;
	}

	return default_val;
}


double
nvs_read_float_default( NVStore *nvs, const char *path, double default_val )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return default_val;
	return atof( val );
}


char *
nvs_read_string_default( NVStore *nvs, const char *path, const char *default_string )
{
	const char *val = nvs_lookup( nvs, path );
	if (val == NULL)
		return xstrdup( default_string );
	return xstrdup( val );
}


void
nvs_write_boolean( NVStore *nvs, const char *path, NVS_BOOL val )
{
	nvs_store( nvs, path, val ? "true" : "false" );
}


void
nvs_write_int( NVStore *nvs, const char *path, int val )
{
	char buf[32];
	sprintf( buf, "%d", val );
	nvs_store( nvs, path, buf );
}


void
nvs_write_int_token( NVStore *nvs, const char *path, int val, const char **tokens )
{
	int i;

	/* Validate token index */
	for (i = 0; tokens[i] != NULL; i++) {
		if (i == val) {
			nvs_store( nvs, path, tokens[val] );
			return;
		}
	}

	/* Fallback: write as integer */
	nvs_write_int( nvs, path, val );
}


void
nvs_write_float( NVStore *nvs, const char *path, double val )
{
	char buf[64];
	sprintf( buf, "%.10g", val );
	nvs_store( nvs, path, buf );
}


void
nvs_write_string( NVStore *nvs, const char *path, const char *string )
{
	nvs_store( nvs, path, string );
}


/* end nvstore.c */
