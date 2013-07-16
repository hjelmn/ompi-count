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
#include "ompi_config.h"
#include <stdio.h>
#include <limits.h>

#include "ompi/mpi/c/bindings.h"
#include "ompi/runtime/params.h"
#include "ompi/communicator/communicator.h"
#include "ompi/errhandler/errhandler.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/memchecker.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Get_elements = PMPI_Get_elements
#pragma weak MPI_Get_elements_x = PMPI_Get_elements_x
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static int _MPI_Get_elements(const char *func_name, MPI_Status *status, MPI_Datatype datatype, MPI_Count *count)
{
    size_t size, internal_count;
    int i;

    OPAL_CR_NOOP_PROGRESS();

    MEMCHECKER(
               if (status != MPI_STATUSES_IGNORE) {
                   /*
                    * Before checking the complete status, we need to reset the definedness
                    * of the MPI_ERROR-field (single-completion calls wait/test).
                    */
                   opal_memchecker_base_mem_defined(&status->MPI_ERROR, sizeof(int));
                   memchecker_status(status);
                   memchecker_datatype(datatype);
               }
               );

    if (MPI_PARAM_CHECK) {
        int err = MPI_SUCCESS;
        OMPI_ERR_INIT_FINALIZE(func_name);
        if (NULL == status || MPI_STATUSES_IGNORE == status || 
            MPI_STATUS_IGNORE == status || NULL == count) {
            err = MPI_ERR_ARG;
        } else if (NULL == datatype || MPI_DATATYPE_NULL == datatype) {
            err = MPI_ERR_TYPE;
        } else {
            OMPI_CHECK_DATATYPE_FOR_RECV(err, datatype, 1);
        }
        OMPI_ERRHANDLER_CHECK(err, MPI_COMM_WORLD, err, func_name);
    }

    *count = 0;
    if( ompi_datatype_type_size( datatype, &size ) == MPI_SUCCESS ) {
        if( size == 0 ) {
            /* If the size of the datatype is zero let's return a count of zero */
            return MPI_SUCCESS;
        }
        internal_count = status->_ucount / size;    /* how many full types? */
        size = status->_ucount - internal_count * size;  /* leftover bytes */
        /* if basic type we should return the same result as MPI_Get_count */
        if( ompi_datatype_is_predefined(datatype) ) {
            if( size != 0 ) {  /* no leftover is supported for predefined types */
                *count = MPI_UNDEFINED;
            }
            goto more_than_int_elements;
        }
        if( internal_count != 0 ) {
            size_t total = 0;  /* count the basic elements in the datatype */
            for( i = 4; i < OPAL_DATATYPE_MAX_PREDEFINED; i++ ) {
                total += datatype->super.btypes[i];
            }
            internal_count = total * internal_count;
        }
        if( size > 0 ) {
            /* If there are any leftover bytes, compute the number of predefined
             * types in the datatype that can fit in these bytes.
             */
            if( (i = ompi_datatype_get_element_count( datatype, size )) != -1 )
                internal_count += i;
            else {
                *count = MPI_UNDEFINED;
                return MPI_SUCCESS;
            }
        }
        goto more_than_int_elements;
    }
    return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_ARG, func_name);
 more_than_int_elements:
    if( internal_count > (size_t) MPI_COUNT_MAX ) {
        /* We have more elements that we can represent with a ssize_t. We must
         * set count to MPI_UNDEFINED (MPI 3.0). We should still be able to return
         * MPI_ERR_TRUNCATE here.
         */
        *count = MPI_UNDEFINED;
    } else {
        *count = (ssize_t) internal_count;
    }

    return MPI_SUCCESS;
}

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

int MPI_Get_elements_x(MPI_Status *status, MPI_Datatype datatype, MPI_Count *count)
{
    return _MPI_Get_elements ("MPI_Get_elements_x", status, datatype, count);
}
