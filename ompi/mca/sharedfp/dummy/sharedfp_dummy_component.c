/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008-2011 University of Houston. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "ompi_config.h"
#include "sharedfp_dummy.h"
#include "mpi.h"

/*
 * Public string showing the sharedfp dummy component version number
 */
const char *mca_sharedfp_dummy_component_version_string =
  "OMPI/MPI dummy SHAREDFP MCA component version " OMPI_VERSION;

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
mca_sharedfp_base_component_2_0_0_t mca_sharedfp_dummy_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    {
        MCA_SHAREDFP_BASE_VERSION_2_0_0,

        /* Component name and version */
        "dummy",
        OMPI_MAJOR_VERSION,
        OMPI_MINOR_VERSION,
        OMPI_RELEASE_VERSION,
        NULL,
        NULL
    },
    {
        /* This component is checkpointable */
      MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    mca_sharedfp_dummy_component_init_query,      /* get thread level */
    mca_sharedfp_dummy_component_file_query,      /* get priority and actions */
    mca_sharedfp_dummy_component_file_unquery     /* undo what was done by previous function */
};
