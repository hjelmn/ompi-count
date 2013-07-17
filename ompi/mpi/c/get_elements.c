/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2010 The University of Tennessee and The University
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

#include "ompi/mpi/c/get_elements.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Get_elements = PMPI_Get_elements
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

int MPI_Get_elements(MPI_Status *status, MPI_Datatype datatype, int *count) 
{
    MPI_Count lcount;
    int rc;

    rc = _MPI_Get_elements ("MPI_Get_elements", status, datatype, &lcount);
    if (lcount > (MPI_Count) INT_MAX ) {
        /* We have more elements that we can represent with a signed int. We must
         * set count to MPI_UNDEFINED (MPI 3.0). We should still be able to return
         * MPI_ERR_TRUNCATE here.
         */
        *count = MPI_UNDEFINED;
    } else {
        *count = (int) lcount;
    }

    return rc;
}
