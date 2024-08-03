/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#include <sstmac/replacements/mpi/mpi.h>

namespace exitst2 {
/** 
 * This is a special test to check that mpiexec handles zero/non-zero 
 * return status from an application.  In this case, each process 
 * returns a different return status
 */
int exitst2( int argc, char *argv[] )
{
    int rank;
    MPI_Init( 0, 0 );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Finalize( );
    return rank;
}

}
