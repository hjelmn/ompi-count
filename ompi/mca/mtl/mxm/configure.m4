/*
 * Copyright (C) Mellanox Technologies Ltd. 2001-2011.  ALL RIGHTS RESERVED.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

# MCA_mtl_mxm_CONFIG([action-if-can-compile],
#                    [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_ompi_mtl_mxm_CONFIG],[
    AC_CONFIG_FILES([ompi/mca/mtl/mxm/Makefile])

    OMPI_CHECK_MXM([mtl_mxm],
                   [mtl_mxm_happy="yes"],
                   [mtl_mxm_happy="no"])

    AS_IF([test "$mtl_mxm_happy" = "yes"],
          [$1],
          [$2])

    # substitute in the things needed to build mxm
    AC_SUBST([mtl_mxm_CFLAGS])
    AC_SUBST([mtl_mxm_CPPFLAGS])
    AC_SUBST([mtl_mxm_LDFLAGS])
    AC_SUBST([mtl_mxm_LIBS])
])dnl

