/* dictd.c -- 
 * Created: Fri Feb 21 20:09:09 1997 by faith@cs.unc.edu
 * Revised: Sat Mar  8 18:38:15 1997 by faith@cs.unc.edu
 * Copyright 1997 Rickard E. Faith (faith@cs.unc.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * 
 * $Id: dictd.c,v 1.7 1997/03/09 00:20:10 faith Exp $
 * 
 */

#include "dictd.h"
#include "servparse.h"

extern int yy_flex_debug;

extern int _dict_comparisons;

static dictConfig *DictConfig;
static const char *DictHostname;

void dict_set_config( dictConfig *dc ) {
   DictConfig = dc;
}

dictConfig *dict_get_config( void ) {
   return DictConfig;
}

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

static void dict_set_hostname( void )
{
   char           myname[MAXHOSTNAMELEN+1];
   struct hostent *hp;
	
   gethostname(myname, sizeof(myname));
   
   if (!(hp = gethostbyname(myname))) {
      DictHostname = "localhost";
      return;
   }
   DictHostname = strdup( hp->h_name );
}

const char *dict_get_hostname( void )
{
   return DictHostname;
}

static void reaper( int dummy )
{
   union wait status;
   pid_t      pid;

   while ((pid = wait3(&status, WNOHANG, NULL)) > 0)
      fprintf( stderr, "Reaped %d\n", pid );
}

struct access_print_struct {
   FILE *s;
   int  offset;
};

static int access_print( const void *datum, void *arg )
{
   dictAccess                 *a     = (dictAccess *)datum;
   struct access_print_struct *aps   = (struct access_print_struct *)arg;
   FILE                       *s     = aps->s;
   int                        offset = aps->offset;
   int                        i;

   for (i = 0; i < offset; i++) fputc( ' ', s );
   fprintf( s,
	    "%s %s\n", a->allow == DICT_ALLOW ? "allow" : "deny ", a->spec );
   return 0;
}

static void acl_print( FILE *s, lst_List l, int offset)
{
   struct access_print_struct aps;
   int                        i;

   aps.s      = s;
   aps.offset = offset + 3;
   
   for (i = 0; i < offset; i++) fputc( ' ', s );
   fprintf( s, "access {\n" );
   lst_iterate_arg( l, access_print, &aps );
   for (i = 0; i < offset; i++) fputc( ' ', s );
   fprintf( s, "}\n" );
}

static int config_print( const void *datum, void *arg )
{
   dictDatabase *db = (dictDatabase *)datum;
   FILE         *s  = (FILE *)arg;

   fprintf( s, "database %s {\n", db->databaseName );
   if (db->dataFilename)
      fprintf( s, "   data       %s\n", db->dataFilename );
   if (db->indexFilename)
      fprintf( s, "   index      %s\n", db->indexFilename );
   if (db->filter)
      fprintf( s, "   filter     %s\n", db->filter );
   if (db->prefilter)
      fprintf( s, "   prefilter  %s\n", db->prefilter );
   if (db->postfilter)
      fprintf( s, "   postfilter %s\n", db->postfilter );
   if (db->acl) acl_print( s, db->acl, 3 );
   fprintf( s, "}\n" );
   return 0;
}

static void dict_config_print( FILE *stream, dictConfig *c )
{
   FILE *s = stream ? stream : stderr;

   if (c->acl) acl_print( s, c->acl, 0 );
   lst_iterate_arg( c->dbl, config_print, s );
}

static int init_database( const void *datum )
{
   dictDatabase *db = (dictDatabase *)datum;
   dictWord     *dw;
   lst_List     list;
   char         *pt;

   db->index = dict_index_open( db->indexFilename );
   db->data  = dict_data_open( db->dataFilename, 0 );

   list = dict_search_database( "!short!", db, DICT_EXACT );
   if (list && lst_length(list) == 1) {
      dw = lst_nth_get( list, 1 );
				/* Don't ever free this */
      pt = dict_data_read( db->data, dw->start, dw->end,
			   db->prefilter, db->postfilter );
      pt += 8;
      while (*pt == ' ' || *pt == '\t') ++pt;
      pt[ strlen(pt) - 1 ] = '\0';
      db->databaseShort = pt;
   } else {
      db->databaseShort = strdup( db->databaseName );
   }
   
   PRINTF(DBG_INIT,
	  ("%s \"%s\" initialized\n",db->databaseName,db->databaseShort));
   return 0;
}

static void dict_init_databases( dictConfig *c )
{
   lst_iterate( c->dbl, init_database );
}

static int dump_def( const void *datum, void *arg )
{
   char         *buf;
   dictWord     *dw = (dictWord *)datum;
   dictDatabase *db = (dictDatabase *)arg;

   buf = dict_data_read( db->data, dw->start, dw->end,
			 db->prefilter, db->postfilter );
   printf( "%s\n", buf );
   xfree( buf );
   return 0;
}

static void dict_dump_defs( lst_List list, dictDatabase *db )
{
   lst_iterate_arg( list, dump_def, db );
}

static const char *id_string( const char *id )
{
   static char buffer[BUFFERSIZE];
   arg_List a = arg_argify( id );

   if (arg_count(a) >= 2)
      sprintf( buffer, "%s", arg_get( a, 2 ) );
   else
      buffer[0] = '\0';
   arg_destroy( a );
   return buffer;
}

const char *dict_get_banner( void )
{
   static char       *buffer= NULL;
   const char        *id = "$Id: dictd.c,v 1.7 1997/03/09 00:20:10 faith Exp $";
   struct utsname    uts;
   
   if (buffer) return buffer;
   uname( &uts );
   buffer = xmalloc(256);
   sprintf( buffer,
	    "%s %s (%s %s)", err_program_name(), id_string( id ),
	    uts.sysname,
	    uts.release );
   return buffer;
}

static void banner( void )
{
   fprintf( stderr, "%s\n", dict_get_banner() );
   fprintf( stderr,
	    "Copyright 1996,1997 Rickard E. Faith (faith@cs.unc.edu)\n" );
}

static void license( void )
{
   static const char *license_msg[] = {
     "",
     "This program is free software; you can redistribute it and/or modify it",
     "under the terms of the GNU General Public License as published by the",
     "Free Software Foundation; either version 1, or (at your option) any",
     "later version.",
     "",
     "This program is distributed in the hope that it will be useful, but",
     "WITHOUT ANY WARRANTY; without even the implied warranty of",
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU",
     "General Public License for more details.",
     "",
     "You should have received a copy of the GNU General Public License along",
     "with this program; if not, write to the Free Software Foundation, Inc.,",
     "675 Mass Ave, Cambridge, MA 02139, USA.",
   };
   const char        **p = license_msg;
   
   banner();
   while (*p) fprintf( stderr, "   %s\n", *p++ );
}
    
static void help( void )
{
   static const char *help_msg[] = {
      "-h --help            give this help",
      "-L --license         display software license",
      "-v --verbose         verbose mode",
      "-V --version         display version number",
      "-D --debug <option>  select debug option",
      "-p --port <port>     port number",
      "-c --config <file>   configuration file",
      "-t --test <word>     self test",
      0 };
   const char        **p = help_msg;

   banner();
   while (*p) fprintf( stderr, "%s\n", *p++ );
}

int main( int argc, char **argv )
{
   int                masterSocket;
   int                childSocket;
   struct sockaddr_in csin;
   int                alen        = sizeof(csin);
   pid_t              pid;
   struct sigaction   sa;
   int                c;
   const char         *service    = DICT_DEFAULT_SERVICE;
   const char         *configFile = DICT_CONFIG_FILE;
   int                detach      = 0;
   const char         *testWord   = NULL;
   const char         *testFile   = NULL;
   struct option      longopts[]  = {
      { "verbose", 0, 0, 'v' },
      { "version", 0, 0, 'V' },
      { "detach",  0, 0, 'd' },
      { "debug",   1, 0, 'D' },
      { "port",    1, 0, 'p' },
      { "config",  1, 0, 'c' },
      { "help",    0, 0, 'h' },
      { "license", 0, 0, 'L' },
      { "test",    1, 0, 't' },
      { "ftest",   1, 0, 501 },
      { 0,         0, 0,  0  }
   };

   maa_init(argv[0]);

   dbg_register( DBG_VERBOSE, "verbose" );
   dbg_register( DBG_PARSE,   "parse" );
   dbg_register( DBG_SCAN,    "scan" );
   dbg_register( DBG_SEARCH,  "search" );
   dbg_register( DBG_INIT,    "init" );

   while ((c = getopt_long( argc, argv,
			    "vVdD:p:c:hLt:", longopts, NULL )) != EOF)
      switch (c) {
      case 'v': dbg_set( "verbose" ); break;
      case 'V': banner(); exit(1);    break;
      case 'd': ++detach;             break;
      case 'D': dbg_set( optarg );    break;
      case 'p': service = optarg;     break;
      case 'c': configFile = optarg;  break;
      case 'L': license(); exit(1);   break;
      case 't': testWord = optarg;    break;
      case 501: testFile = optarg;    break;
      case 'h':
      default:  help(); exit(1);      break;
      }

   if (dbg_test(DBG_PARSE))     prs_set_debug(1);
   if (dbg_test(DBG_SCAN))      yy_flex_debug = 1;
   else                         yy_flex_debug = 0;

   DictConfig = xmalloc(sizeof(struct dictConfig));
   prs_file_nocpp( configFile );
   dict_config_print( NULL, DictConfig );
   dict_init_databases( DictConfig );
   dict_set_hostname();

   if (testWord) {		/* stand-alone test mode */
      lst_List list = dict_search_database( testWord,
					    lst_nth_get( DictConfig->dbl, 1 ),
					    DICT_EXACT );
      if (list) {
	 if (dbg_test(DBG_VERBOSE)) dict_dump_list( list );
	 dict_dump_defs( list, lst_nth_get( DictConfig->dbl, 1 ) );
	 dict_destroy_list( list );
      } else {
	 printf( "No match\n" );
      }
      fprintf( stderr, "%d comparisons\n", _dict_comparisons );
      exit( 0 );
   }

   if (testFile) {
      FILE         *str;
      char         buf[1024];
      dictDatabase *db = lst_nth_get(DictConfig->dbl, 1);
      int          words = 0;

      if (!(str = fopen(testFile,"r")))
	 err_fatal_errno( "Cannot open \"%s\" for read\n", testFile );
      while (fgets(buf,1024,str)) {
	 lst_List list = dict_search_database( buf, db, DICT_EXACT );
	 ++words;
	 if (list) {
	    if (dbg_test(DBG_VERBOSE)) dict_dump_list( list );
	    dict_dump_defs( list, db );
	    dict_destroy_list( list );
	 } else {
	    fprintf( stderr, "*************** No match for \"%s\"\n", buf );
	 }
	 if (words && !(words % 1000))
	    fprintf( stderr,
		     "%d comparisons, %d words\n", _dict_comparisons, words );
      }
      fprintf( stderr,
	       "%d comparisons, %d words\n", _dict_comparisons, words );
      fclose( str);
      exit(0);
   }
   
   sa.sa_handler = reaper;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(SIGCHLD,&sa,NULL);

   fflush(stdout);
   fflush(stderr);

   masterSocket = net_open_tcp( service, DICT_QUEUE_DEPTH );

   for (;;) {
      if ((childSocket = accept(masterSocket,
				(struct sockaddr *)&csin, &alen)) < 0) {
	 if (errno == EINTR) continue;
	 err_fatal_errno( __FUNCTION__, "Can't accept" );
      }
      switch ((pid = fork())) {
      case 0:			/* child */
	 close(masterSocket);
	 exit(dict_daemon(childSocket,&csin));
      case -1:
	 err_fatal_errno( __FUNCTION__, "Fork failed" );
      default:
	 fprintf( stderr, "Forked %d\n", pid );
	 close(childSocket);
	 break;
      }
   }
}