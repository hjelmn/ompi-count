/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2008 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2013      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mpi/c/bindings.h"
#include "ompi/runtime/params.h"
#include "ompi/communicator/communicator.h"
#include "ompi/errhandler/errhandler.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/memchecker.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Type_get_extent = PMPI_Type_get_extent
#pragma weak MPI_Type_get_extent_x = PMPI_Type_get_extent_x
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static int _MPI_Type_get_extent(const char *func_name, MPI_Datatype type, MPI_Aint *lb, MPI_Aint *extent)
{
  int rc;

  MEMCHECKER(
    memchecker_datatype(type);
  );
  
  if (MPI_PARAM_CHECK) {
    OMPI_ERR_INIT_FINALIZE(func_name);
    if (NULL == type || MPI_DATATYPE_NULL == type) {
      return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_TYPE, func_name);
    } else if (NULL == lb || NULL == extent) {
      return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_ARG, func_name);
    }
  }

  OPAL_CR_ENTER_LIBRARY();

  rc = ompi_datatype_get_extent( type, lb, extent );
  OMPI_ERRHANDLER_RETURN(rc, MPI_COMM_WORLD, rc, func_name );
}

int MPI_Type_get_extent(MPI_Datatype type, MPI_Aint *lb, MPI_Aint *extent)
{
  return _MPI_Type_get_extent("MPI_Type_get_extent", type, lb, extent);
}

int MPI_Type_get_extent_x(MPI_Datatype type, MPI_Count *lb, MPI_Count *extent)
{
  MPI_Aint alb, aextent;
  int rc;

  rc = _MPI_Type_get_extent("MPI_Type_get_extent_x", type, &alb, &aextent);
  if (MPI_SUCCESS == rc) {    
    *lb = ((size_t) alb > MPI_COUNT_MAX) ? MPI_UNDEFINED : alb;
    *extent = ((size_t) aextent > MPI_COUNT_MAX) ? MPI_UNDEFINED : aextent;
  }

  return rc;
}
