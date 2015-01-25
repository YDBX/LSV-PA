/**CFile****************************************************************

  FileName    [extraUtilPrime.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [Function enumeration.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: extraUtilPrime.c,v 1.0 2003/02/01 00:00:00 alanmi Exp $]

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "misc/vec/vec.h"
#include "misc/vec/vecHsh.h"
#include "bool/kit/kit.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_GenCountDump( Vec_Int_t * vPrimes, int nVars, char * pFileName )
{
    FILE * pFile;
    int i, k, Prime;
    pFile = fopen( pFileName, "wb" );
    fprintf( pFile, "# %d prime numbers up to 2^%d generated by ABC on %s\n", Vec_IntSize(vPrimes), nVars, Extra_TimeStamp() );
    fprintf( pFile, ".i %d\n", nVars );
    fprintf( pFile, ".o %d\n", 1 );
    fprintf( pFile, ".p %d\n", Vec_IntSize(vPrimes) );
    Vec_IntForEachEntry( vPrimes, Prime, i )
        for ( k = nVars-1; k >= 0; k-- )
            fprintf( pFile, "%d%s", (Prime >> k)&1, k ? "" : " 1\n" );
    fprintf( pFile, ".e\n\n" );
    fclose( pFile );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_GenCountHits1( Vec_Bit_t * vMap, Vec_Int_t * vPrimes, int nVars )
{
    abctime clk = Abc_Clock();
    int i, k, Prime, Count = 0;
    Vec_IntForEachEntry( vPrimes, Prime, i )
    {
        for ( k = 0; k < nVars; k++ )
            if ( !Vec_BitEntry(vMap, Prime ^ (1<<k)) )
                Count++;
    }
    printf( "Dist1 pairs = %d. ", Count );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clk );
}
Vec_Int_t * Abc_GenPrimes( int nVars )
{
    int i, n, nBits = ( 1 << nVars );
    Vec_Bit_t * vMap = Vec_BitStart( nBits );
    Vec_Int_t * vPrimes = Vec_IntAlloc( 1000 );
    for ( n = 2; n < nBits; n++ )
        if ( !Vec_BitEntry(vMap, n) )
            for ( i = 2*n; i < nBits; i += n )
                Vec_BitWriteEntry(vMap, i, 1);
    for ( n = 2; n < nBits; n++ )
        if ( !Vec_BitEntry(vMap, n) )
            Vec_IntPush( vPrimes, n );
    Abc_GenCountHits1( vMap, vPrimes, nVars );
    Vec_BitFree( vMap );
    return vPrimes;
}
void Abc_GenPrimesTest()
{
    // 54,400,028 primes up to 2^30 can be computed in 22 sec
    int nVars = 18;
    Vec_Int_t * vPrimes = Abc_GenPrimes( nVars );
    Abc_GenCountDump( vPrimes, nVars, "primes18.pla" );
    //Vec_IntPrint( vPrimes );
    printf( "Primes up to 2^%d = %d\n", nVars, Vec_IntSize(vPrimes) );

    Vec_IntFree( vPrimes );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

