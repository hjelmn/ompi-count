/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi/mpi/tool/mpit-internal.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_T_pvar_session_create = PMPI_T_pvar_session_create
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/tool/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_T_pvar_session_create";

int MPI_T_pvar_session_create(MPI_T_pvar_session *session)
{
    int ret = MPI_SUCCESS;

    if (!mpit_is_initialized ()) {
        return MPI_T_ERR_NOT_INITIALIZED;
    }

    mpit_lock ();

    do {
        *session = OBJ_NEW(mca_base_pvar_session_t);
        if (NULL == *session) {
            ret = MPI_ERR_NO_MEM;
            break;
        }
    } while (0);

    mpit_unlock ();

    return ret;
}
