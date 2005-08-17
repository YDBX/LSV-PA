/**CFile****************************************************************

  FileName    [abcFunc.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Transformations between different functionality representations.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcFunc.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static DdNode *    Abc_ConvertSopToBdd( DdManager * dd, char * pSop, int nFanins );
static int         Abc_ConvertZddToSop( DdManager * dd, DdNode * zCover, char * pSop, int nFanins, Vec_Str_t * vCube, int fPhase );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFITIONS                           ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Converts the network from SOP to BDD representation.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkSopToBdd( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    DdManager * dd;
    int nFaninsMax, i;

    assert( Abc_NtkIsLogicSop(pNtk) ); 

    // start the functionality manager
    nFaninsMax = Abc_NtkGetFaninMax( pNtk );
    if ( nFaninsMax == 0 )
        printf( "Warning: The network has only constant nodes.\n" );

    dd = Cudd_Init( nFaninsMax, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0 );

    // convert each node from SOP to BDD
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        assert( pNode->pData );
        pNode->pData = Abc_ConvertSopToBdd( dd, pNode->pData, Abc_ObjFaninNum(pNode) );
        if ( pNode->pData == NULL )
        {
            printf( "Abc_NtkSopToBdd: Error while converting SOP into BDD.\n" );
            return 0;
        }
        Cudd_Ref( pNode->pData );
    }

    Extra_MmFlexStop( pNtk->pManFunc, 0 );
    pNtk->pManFunc = dd;

    // update the network type
    pNtk->Type = ABC_NTK_LOGIC_BDD;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Converts the node from SOP to BDD representation.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
DdNode * Abc_ConvertSopToBdd( DdManager * dd, char * pSop, int nFanins )
{
    DdNode * bCube, * bTemp, * bVar, * bRes;
    char * pCube;
    int i, c;

    bRes = Cudd_Not(dd->one);   Cudd_Ref( bRes );
    for ( c = 0; ; c++ )
    {
        // get the cube
        pCube = pSop + c * (nFanins + 3);
        if ( *pCube == 0 )
            break;
        // construct BDD for the cube
        bCube = dd->one;   Cudd_Ref( bCube );
        for ( i = 0; i < nFanins; i++ )
        {
            if ( pCube[i] == '0' )
                bVar = Cudd_Not( dd->vars[i] );
            else if ( pCube[i] == '1' )
                bVar = dd->vars[i];
            else
                continue;
            bCube  = Cudd_bddAnd( dd, bTemp = bCube, bVar );   Cudd_Ref( bCube );
            Cudd_RecursiveDeref( dd, bTemp );
        }
        bRes = Cudd_bddOr( dd, bTemp = bRes, bCube );   Cudd_Ref( bRes );
        Cudd_RecursiveDeref( dd, bTemp );
        Cudd_RecursiveDeref( dd, bCube );
    }
    // decide if we need to complement the result
    pCube = pSop + nFanins + 1;
    assert( *pCube == '0' || *pCube == '1' );
    if ( *pCube == '0' )
        bRes = Cudd_Not(bRes);
    Cudd_Deref( bRes );
    return bRes;
}






/**Function*************************************************************

  Synopsis    [Converts the network from BDD to SOP representation.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkBddToSop( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    DdManager * dd = pNtk->pManFunc;
    DdNode * bFunc;
    int RetValue, i;
    Vec_Str_t * vCube;

    assert( Abc_NtkIsLogicBdd(pNtk) ); 
    Cudd_zddVarsFromBddVars( dd, 2 );
    // allocate the new manager
    pNtk->pManFunc = Extra_MmFlexStart();
    // update the network type
    pNtk->Type = ABC_NTK_LOGIC_SOP;

    // go through the objects
    vCube = Vec_StrAlloc( 100 );
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        assert( pNode->pData );
        bFunc = pNode->pData;
//Extra_bddPrint( dd, bFunc ); printf( "\n" ); printf( "\n" );
        pNode->pData = Abc_ConvertBddToSop( pNtk->pManFunc, dd, bFunc, bFunc, Abc_ObjFaninNum(pNode), vCube, -1 );
        if ( pNode->pData == NULL )
            return 0;
        Cudd_RecursiveDeref( dd, bFunc );
    }
    Vec_StrFree( vCube );

    // check for remaining references in the package
    RetValue = Cudd_CheckZeroRef( dd );
    if ( RetValue > 0 )
        printf( "\nThe number of referenced nodes = %d\n\n", RetValue );
//  Cudd_PrintInfo( dd, stdout );
    Cudd_Quit( dd );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Converts the node from BDD to SOP representation.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Abc_ConvertBddToSop( Extra_MmFlex_t * pMan, DdManager * dd, DdNode * bFuncOn, DdNode * bFuncOnDc, int nFanins, Vec_Str_t * vCube, int fMode )
{
    int fVerify = 0;
    char * pSop;
    DdNode * bFuncNew, * bCover, * zCover, * zCover0, * zCover1;
    int nCubes, nCubes0, nCubes1, fPhase;

    assert( bFuncOn == bFuncOnDc || Cudd_bddLeq( dd, bFuncOn, bFuncOnDc ) );
    if ( Cudd_IsConstant(bFuncOn) || Cudd_IsConstant(bFuncOnDc) )
    {
        Vec_StrFill( vCube, nFanins, '-' );
        Vec_StrPush( vCube, '\0' );
        pSop = Extra_MmFlexEntryFetch( pMan, nFanins + 4 );
        if ( bFuncOn == Cudd_ReadLogicZero(dd) )
            sprintf( pSop, "%s 0\n", vCube->pArray );
        else
            sprintf( pSop, "%s 1\n", vCube->pArray );
        return pSop;
    }


    if ( fMode == -1 )
    { // try both phases

        // get the ZDD of the negative polarity
        bCover = Cudd_zddIsop( dd, Cudd_Not(bFuncOnDc), Cudd_Not(bFuncOn), &zCover0 );
        Cudd_Ref( zCover0 );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes0 = Abc_CountZddCubes( dd, zCover0 );

        // get the ZDD of the positive polarity
        bCover = Cudd_zddIsop( dd, bFuncOn, bFuncOnDc, &zCover1 );
        Cudd_Ref( zCover1 );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes1 = Abc_CountZddCubes( dd, zCover1 );

        // compare the number of cubes
        if ( nCubes1 <= nCubes0 )
        { // use positive polarity
            nCubes = nCubes1;
            zCover = zCover1;
            Cudd_RecursiveDerefZdd( dd, zCover0 );
            fPhase = 1;
        }
        else
        { // use negative polarity
            nCubes = nCubes0;
            zCover = zCover0;
            Cudd_RecursiveDerefZdd( dd, zCover1 );
            fPhase = 0;
        }
    }
    else if ( fMode == 0 )
    {
        // get the ZDD of the positive polarity
        bCover = Cudd_zddIsop( dd, Cudd_Not(bFuncOnDc), Cudd_Not(bFuncOn), &zCover );
        Cudd_Ref( zCover );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes = Abc_CountZddCubes( dd, zCover );
        fPhase = 0;
    }
    else if ( fMode == 1 )
    {
        // get the ZDD of the positive polarity
        bCover = Cudd_zddIsop( dd, bFuncOn, bFuncOnDc, &zCover );
        Cudd_Ref( zCover );
        Cudd_Ref( bCover );
        Cudd_RecursiveDeref( dd, bCover );
        nCubes = Abc_CountZddCubes( dd, zCover );
        fPhase = 1;
    }
    else
    {
        assert( 0 );
    }

    // allocate memory for the cover
    if ( pMan )
        pSop = Extra_MmFlexEntryFetch( pMan, (nFanins + 3) * nCubes + 1 );
    else 
        pSop = ALLOC( char, (nFanins + 3) * nCubes + 1 );
    pSop[(nFanins + 3) * nCubes] = 0;
    // create the SOP
    Vec_StrFill( vCube, nFanins, '-' );
    Vec_StrPush( vCube, '\0' );
    Abc_ConvertZddToSop( dd, zCover, pSop, nFanins, vCube, fPhase );
    Cudd_RecursiveDerefZdd( dd, zCover );

    // verify
    if ( fVerify )
    {
        bFuncNew = Abc_ConvertSopToBdd( dd, pSop, nFanins );  Cudd_Ref( bFuncNew );
        if ( bFuncOn == bFuncOnDc )
        {
            if ( bFuncNew != bFuncOn )
                printf( "Verification failed.\n" );
        }
        else
        {
            if ( !Cudd_bddLeq(dd, bFuncOn, bFuncNew) || !Cudd_bddLeq(dd, bFuncNew, bFuncOnDc) )
                printf( "Verification failed.\n" );
        }
        Cudd_RecursiveDeref( dd, bFuncNew );
    }
    return pSop;
}

/**Function*************************************************************

  Synopsis    [Derive the SOP from the ZDD representation of the cubes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ConvertZddToSop_rec( DdManager * dd, DdNode * zCover, char * pSop, int nFanins, Vec_Str_t * vCube, int fPhase, int * pnCubes )
{
    DdNode * zC0, * zC1, * zC2;
    int Index;

    if ( zCover == dd->zero )
        return;
    if ( zCover == dd->one )
    {
        char * pCube;
        pCube = pSop + (*pnCubes) * (nFanins + 3);
        sprintf( pCube, "%s %d\n", vCube->pArray, fPhase );
        (*pnCubes)++;
        return;
    }
    Index = zCover->index/2;
    assert( Index < nFanins );
    extraDecomposeCover( dd, zCover, &zC0, &zC1, &zC2 );
    vCube->pArray[Index] = '0';
    Abc_ConvertZddToSop_rec( dd, zC0, pSop, nFanins, vCube, fPhase, pnCubes );
    vCube->pArray[Index] = '1';
    Abc_ConvertZddToSop_rec( dd, zC1, pSop, nFanins, vCube, fPhase, pnCubes );
    vCube->pArray[Index] = '-';
    Abc_ConvertZddToSop_rec( dd, zC2, pSop, nFanins, vCube, fPhase, pnCubes );
}

/**Function*************************************************************

  Synopsis    [Derive the BDD for the function in the cut.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_ConvertZddToSop( DdManager * dd, DdNode * zCover, char * pSop, int nFanins, Vec_Str_t * vCube, int fPhase )
{
    int nCubes = 0;
    Abc_ConvertZddToSop_rec( dd, zCover, pSop, nFanins, vCube, fPhase, &nCubes );
    return nCubes;
}


/**Function*************************************************************

  Synopsis    [Computes the SOPs of the negative and positive phase of the node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeBddToCnf( Abc_Obj_t * pNode, Extra_MmFlex_t * pMmMan, Vec_Str_t * vCube, char ** ppSop0, char ** ppSop1 )
{
    assert( Abc_NtkIsLogicBdd(pNode->pNtk) ); 
    *ppSop0 = Abc_ConvertBddToSop( pMmMan, pNode->pNtk->pManFunc, pNode->pData, pNode->pData, Abc_ObjFaninNum(pNode), vCube, 0 );
    *ppSop1 = Abc_ConvertBddToSop( pMmMan, pNode->pNtk->pManFunc, pNode->pData, pNode->pData, Abc_ObjFaninNum(pNode), vCube, 1 );
}




/**Function*************************************************************

  Synopsis    [Count the number of paths in the ZDD.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_CountZddCubes_rec( DdManager * dd, DdNode * zCover, int * pnCubes )
{
    DdNode * zC0, * zC1, * zC2;
    if ( zCover == dd->zero )
        return;
    if ( zCover == dd->one )
    {
        (*pnCubes)++;
        return;
    }
    extraDecomposeCover( dd, zCover, &zC0, &zC1, &zC2 );
    Abc_CountZddCubes_rec( dd, zC0, pnCubes );
    Abc_CountZddCubes_rec( dd, zC1, pnCubes );
    Abc_CountZddCubes_rec( dd, zC2, pnCubes );
}

/**Function*************************************************************

  Synopsis    [Count the number of paths in the ZDD.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CountZddCubes( DdManager * dd, DdNode * zCover )
{
    int nCubes = 0;
    Abc_CountZddCubes_rec( dd, zCover, &nCubes );
    return nCubes;
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


