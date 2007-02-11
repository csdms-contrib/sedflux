//---
//
// This file is part of sedflux.
//
// sedflux is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// sedflux is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with sedflux; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//---

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "sed_sedflux.h"
#include "processes.h"
#include "plume_types.h"
#include "utils.h"

/** Global variable that lists all of the process that sedflux will run 

sedflux will cycle through the process in the order that they are listed here.
To add a new process to sedflux, you will need to add an entry to this list.
sedflux will will then automatically cycle through your process.
*/
static Sed_process_init_t process_list[] =
{
   { "constants"         , sizeof(Constants_t)    , init_constants     , run_constants   } , 
   { "earthquake"        , sizeof(Quake_t)        , init_quake         , run_quake       } , 
   { "tide"              , sizeof(Tide_t)         , init_tide          , run_tide        } , 
   { "sea level"         , sizeof(Sea_level_t)    , init_sea_level     , run_sea_level   } ,
   { "storms"            , sizeof(Storm_t)        , init_storm         , run_storm       } ,
   { "erosion"           , sizeof(Erosion_t)      , init_erosion       , run_erosion     } ,
   { "river"             , sizeof(River_t)        , init_river         , run_river       } ,
   { "avulsion"          , sizeof(Avulsion_t)     , init_avulsion      , run_avulsion    } ,

   /* A new process */
   { "new process"       , sizeof(New_process_t)  , init_new_process   , run_new_process } ,

   /* The rest of the processes */
   { "bedload dumping"   , sizeof(Bedload_dump_t) , init_bedload       , run_bedload     } ,
   { "plume"             , sizeof(Plume_t)        , init_plume         , run_plume       } ,
   { "turbidity current" , sizeof(Turbidity_t)    , init_turbidity     , run_turbidity   } ,
   { "debris flow"       , sizeof(Debris_flow_t)  , init_debris_flow   , run_debris_flow } ,
   { "slump"             , sizeof(Slump_t)        , init_slump         , run_slump       } ,
   { "diffusion"         , sizeof(Diffusion_t)    , init_diffusion     , run_diffusion   } ,
   { "xshore"            , sizeof(Xshore_t)       , init_xshore        , run_xshore      } ,
   { "squall"            , sizeof(Squall_t)       , init_squall        , run_squall      } ,
   { "compaction"        , sizeof(Compaction_t)   , init_compaction    , run_compaction  } ,
   { "flow"              , sizeof(Flow_t)         , init_flow          , run_flow        } ,
   { "isostasy"          , sizeof(Isostasy_t)     , init_isostasy      , run_isostasy    } ,
   { "subsidence"        , sizeof(Subsidence_t)   , init_subsidence    , run_subsidence  } ,
   { "data dump"         , sizeof(Data_dump_t)    , init_data_dump     , run_data_dump   } ,
   { "failure"           , sizeof(Failure_proc_t) , init_failure       , run_failure     } ,
   { "measuring station" , sizeof(Met_station_t)  , init_met_station   , run_met_station } ,
   { "bbl"               , sizeof(Bbl_t)          , init_bbl           , run_bbl         } ,
   { "cpr"               , sizeof(Cpr_t)          , init_cpr           , run_cpr         }
};

/** Create a queue of processes from a file

Create a new queue of process for sedflux to run.  The processes are initialized
using the process file, \p file.

If the string array, \p user_data is non-NULL, it is a NULL-terminated list
of strings that indicate which processes should be active.  This will override
the process file of the epochs.  Any process not listed in \p active
will \b NOT be active.

\param file      The file containing the process information
\param user_data A string array of process to activate (or NULL)

\return A newly-allocated (and initialized) Sed_process_queue.
*/

Sed_process_queue
sedflux_create_process_queue( const gchar* file , gchar** user_data )
{
   Sed_process_queue q = NULL;

   if ( file )
   {
      gssize i;
      Eh_key_file key_file    = eh_key_file_scan( file );
      gint        n_processes = sizeof( process_list ) / sizeof(Sed_process_init_t);

      q = sed_process_queue_new();
      for ( i=0 ; i<n_processes ; i++ )
         sed_process_queue_push( q , process_list[i] );

      sed_process_queue_scan( q , key_file );

      if ( user_data )
      {
         gchar** name;
         sed_process_queue_deactivate( q , "<all>" );

         for ( name=user_data ; *name ; name++ )
            sed_process_queue_activate  ( q , *name );
      }

      eh_key_file_destroy( key_file );

      {
         gssize i;
         Failure_proc_t** data;
         Sed_process d = sed_process_queue_find_nth_obj( q , "debris flow"       , 0 );
         Sed_process t = sed_process_queue_find_nth_obj( q , "turbidity current" , 0 );
         Sed_process s = sed_process_queue_find_nth_obj( q , "slump"             , 0 );

         data = (Failure_proc_t**)sed_process_queue_obj_data( q , "failure" );
         for ( i=0 ; data && data[i] ; i++ )
         {
            data[i]->debris_flow       = d;
            data[i]->turbidity_current = t;
            data[i]->slump             = s;
         }
         eh_free( data );
      }
   }

   return q;
}

typedef struct
{
   const gchar* name;
   gint error;
} Process_check_t;

/** Individual process errors
*/
Process_check_t process_check[] =
{
   {"plume"           , SED_ERROR_MULTIPLE_PROCS|SED_ERROR_INACTIVE|SED_ERROR_NOT_ALWAYS } ,
   {"bedload dumping" , SED_ERROR_MULTIPLE_PROCS|SED_ERROR_INACTIVE|SED_ERROR_NOT_ALWAYS } ,
   {"bbl"             , SED_ERROR_MULTIPLE_PROCS|SED_ERROR_INACTIVE|SED_ERROR_NOT_ALWAYS } ,
   {"river"           ,                          SED_ERROR_INACTIVE|SED_ERROR_NOT_ALWAYS } ,
   {"earthquake"      , SED_ERROR_MULTIPLE_PROCS } ,
   {"storms"          , SED_ERROR_MULTIPLE_PROCS }
};

typedef struct
{
   const gchar* parent_name;
   const gchar* child_name;
   gint error;
} Family_check_t;

/** Process-family errors
*/
Family_check_t family_check[] =
{
   { "failure"   , "earthquake" , SED_ERROR_INACTIVE_PARENT|SED_ERROR_ABSENT_PARENT|SED_ERROR_DT_MISMATCH },
   { "squall"    , "storms" , SED_ERROR_INACTIVE_PARENT|SED_ERROR_ABSENT_PARENT|SED_ERROR_DT_MISMATCH },
   { "xshore"    , "storms" , SED_ERROR_INACTIVE_PARENT|SED_ERROR_ABSENT_PARENT|SED_ERROR_DT_MISMATCH },
   { "diffusion" , "storms" , SED_ERROR_INACTIVE_PARENT|SED_ERROR_ABSENT_PARENT|SED_ERROR_DT_MISMATCH }
};

/** Check a Sed_process_queue for potential errors

The Sed_process_queue is examined for potential errors.  The type of
errors are listed in the variables \p process_check and \p family_check.
\p process_check describes errors that occur in individual processes
while \p family_check describes errors in families of processes.

\param q    A Sed_process_queue

\return TRUE if there were no erros, FALSE otherwise.
*/
gboolean check_process_list( Sed_process_queue q )
{
   gboolean no_errors = TRUE;
   gint error = 0;
   gssize n_checks = sizeof( process_check ) / sizeof( Process_check_t );
   gssize n_families;
   gssize i;

   for ( i=0 ; i<n_checks ; i++ )
   {
      error = sed_process_queue_check( q , process_check[i].name );
      if ( error & process_check[i].error )
      {
         eh_warning( "%s: Possible error (#%d) in process input file." ,
                     process_check[i].name , error );
         no_errors = FALSE;
      }
   }

   n_families = sizeof( family_check ) / sizeof( Family_check_t );

   for ( i=0 ; i<n_families ; i++ )
   {
      error = sed_process_queue_check_family( q                           ,
                                              family_check[i].parent_name ,
                                              family_check[i].child_name , NULL );
      if (   error & family_check[i].error )
      {
         eh_warning( "%s: Possible error (#%d) in process input file." ,
                     family_check[i].parent_name , error );
         no_errors = FALSE;
      }
   }

   return no_errors;
}


/** Check the input process files.

This function will read in all of the epoch files, and initialize the
processes.  This is intended to find any errors in the input files at 
the begining of the model run.

If the string array, \p active is non-NULL, it is a NULL-terminated list
of strings that indicate which process should be active.  This will override
the process file of the epochs.  Any process not listed in \p active
will \b NOT be active.

\param e_list  A singly liked list of pointers to Epoch's.
\param active  If non-NULL, a list of process to activate.  

\return TRUE if there were no errors, FALSE otherwise
*/
gboolean check_process_files( Sed_epoch_queue e_list , gchar** active )
{
   gboolean no_errors = TRUE;
   int n_epochs = sed_epoch_queue_length( e_list );
   gssize i;
   Sed_process_queue q;
   Sed_epoch this_epoch;

   //---
   // For each epoch, we create each of the processes, initialize the 
   // processes from the appropriate input file, and destroy the processes.
   //---
   for ( i=0 ; i<n_epochs ; i++ )
   {
      this_epoch = sed_epoch_queue_nth( e_list , i );

      q = sedflux_create_process_queue( sed_epoch_filename(this_epoch) , active );

      no_errors = no_errors && check_process_list( q );

      sed_process_queue_destroy( q );
   } 

   return no_errors;
}

