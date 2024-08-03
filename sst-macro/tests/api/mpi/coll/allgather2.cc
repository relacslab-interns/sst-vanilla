/*
 *
 *  (C) 2003 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#include <sstmac/replacements/mpi/mpi.h>
#include "mpitest.h"
#include <stdlib.h>
#include <stdio.h>

namespace allgather2 {

/** Gather data from a vector to contiguous.  Use IN_PLACE */

int allgather2( int argc, char **argv )
{
    double *vecout;
    MPI_Comm comm;
    int    count, minsize = 2;
    int    i, errs = 0;
    int    rank, size;
    int worldrank;

    MTest_Init( &argc, &argv );

    MPI_Comm_rank(MPI_COMM_WORLD, &worldrank);

    while (MTestGetIntracommGeneral( &comm, minsize, 1 )) {
	if (comm == MPI_COMM_NULL) continue;
	/** Determine the sender and receiver */
	MPI_Comm_rank( comm, &rank );
	MPI_Comm_size( comm, &size );
	
	//std::cout << "worldrank(" << worldrank << "): my new rank is: " << rank << "\n";
        for (count = 1; count < 9000; count = count * 2) {
            vecout = (double *)malloc( size * count * sizeof(double) );
            
            for (i=0; i<count; i++) {
                vecout[rank*count+i] = rank*count+i;
            }
            MPI_Allgather( MPI_IN_PLACE, -1, MPI_DATATYPE_NULL, 
                           vecout, count, MPI_DOUBLE, comm );
            for (i=0; i<count*size; i++) {
                if (vecout[i] != i) {
                    errs++;
                    if (errs < 10) {
                        fprintf( stderr, "vecout[%d]=%d\n",
                                 i, (int)vecout[i] );
                    }
                }
            }
            free( vecout );
        }

	MTestFreeComm( &comm );
    }
    
    MTest_Finalize( errs );
    MPI_Finalize();
    return 0;
}


}
