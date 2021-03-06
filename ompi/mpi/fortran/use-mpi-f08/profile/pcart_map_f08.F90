! -*- f90 -*-
!
! Copyright (c) 2010-2012 Cisco Systems, Inc.  All rights reserved.
! Copyright (c) 2009-2012 Los Alamos National Security, LLC.
!                         All rights reserved.
! $COPYRIGHT$

subroutine PMPI_Cart_map_f08(comm,ndims,dims,periods,newrank,ierror)
   use :: mpi_f08_types, only : MPI_Comm
   use :: mpi_f08, only : ompi_cart_map_f
   implicit none
   TYPE(MPI_Comm), INTENT(IN) :: comm
   INTEGER, INTENT(IN) :: ndims, dims(ndims)
   LOGICAL, INTENT(IN) :: periods(ndims)
   INTEGER, INTENT(OUT) :: newrank
   INTEGER, OPTIONAL, INTENT(OUT) :: ierror
   integer :: c_ierror

   call ompi_cart_map_f(comm%MPI_VAL,ndims,dims,periods,newrank,c_ierror)
   if (present(ierror)) ierror = c_ierror

end subroutine PMPI_Cart_map_f08
