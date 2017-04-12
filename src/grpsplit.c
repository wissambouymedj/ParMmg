/**
 * \file grpsplit.c
 * \brief Split groups into sub groups.
 * \author Cécile Dobrzynski (Bx INP/Inria)
 * \author Algiane Froehly (Inria)
 * \author Nikos Pattakos (Inria)
 * \version 5
 * \copyright GNU Lesser General Public License.
 * \todo doxygen documentation.
 */
#include "libparmmgtypes.h" // PMMG_pGrp
#include "metis.h" // idx_t
#include "mmgcommon.h" // _MMG5_SAFE_CALLOC
#include "libparmmg.h" // PMMG_mesh2metis
#include "grpsplit.h"

// Subgroups target size. It is chosen arbitrarily to help assist the remesher work faster
static const int REMESHER_TARGET_MESH_SIZE = 2000;

static int HowManyGroups ( const int nelem )
{
  int ngrp = nelem / REMESHER_TARGET_MESH_SIZE;

  if ( ngrp == 0 )
    return ( 1 );
  else if ( ngrp * REMESHER_TARGET_MESH_SIZE < nelem )
    return ( ngrp + 1 );

  return ( ngrp );
}


//NIKOS TODO: there is some code duplication. eg this metis calling is also in the metisfunction.c

/**
 * \param parmesh pointer toward the parmesh structure.
 * \return 0 if fail, 1 otherwise.
 *
 * Split one group into several groups.
 *
 */
int PMMG_splitGrps( PMMG_pParMesh parmesh )
{
  PMMG_pGrp grpsOld = parmesh->listgrp;
  MMG5_pMesh meshOld = grpsOld->mesh;
  PMMG_pGrp grpsNew = NULL;
  int *countPerGrp = NULL;

  idx_t ngrp = 1;
  idx_t *part = NULL;
  idx_t *xadj = NULL;
  idx_t *adjncy = NULL;
//NIKOS TODO: experiment with the number of balancing constraints
  idx_t ncon = 1;/*number of balancing constraint*/
  idx_t nelt = meshOld->ne;
  idx_t objval;
  int ier;

  /* counters for tetra, point, while constructing a subgroup */
  int tetPerGrp = 0;
  int poiPerGrp = 0;

  // Loop counter vars
  int i, grpId, poi, tet;

printf( "+++++NIKOS+++++[%d/%d]: mesh has: %d(%d) #points, %d(%d) #edges, %d(%d) #tria and %d(%d) tetras(elements).",
        parmesh->myrank+1, parmesh->nprocs, meshOld->np, meshOld->npi, meshOld->na, meshOld->nai, meshOld->nt, meshOld->nti, meshOld->ne, meshOld->nei );

  ngrp = HowManyGroups( meshOld->ne );
  /* Does the group need to be further subdivided to subgroups or not? */
  if ( ngrp == 1 )  {
    printf( "[%d-%d]: %d subgroup is needed, no sub groups to create.\n", parmesh->myrank+1, parmesh->nprocs, ngrp );
    return 1;
  } else {
    printf( "[%d-%d]: %d subgroups are needed, creating sub groups...\n", parmesh->myrank+1, parmesh->nprocs, ngrp );
  }

  // use metis to partition the mesh into the computed number of groups needed

  // part array contains the groupID computed by metis for each tetra
  _MMG5_SAFE_CALLOC( part, meshOld->ne, idx_t );

  PMMG_mesh2metis( parmesh, &xadj, &adjncy );

  ier =  METIS_PartGraphKway( &nelt          , &ncon          , xadj , adjncy        , NULL/*vwgt*/ ,
                              NULL/*vsize*/  , NULL/*adjwgt*/ , &ngrp, NULL/*tpwgts*/, NULL/*ubvec*/,
                              NULL/*options*/, &objval, part );
  //ier =  METIS_PartGraphRecursive( &nelt, &ncon, xadj, adjncy, NULL/\*vwgt*\/, NULL/\*vsize*\/,
  //                                 NULL/\*adjwgt*\/, &nproc, NULL/\*tpwgts*\/,
  //                                 NULL/\*ubvec*\/, NULL/\*options*\/, &objval, part );
  if ( ier != METIS_OK ) {
    fprintf(stderr, "Metis returned error value: " );
    switch ( ier ) {
      case METIS_ERROR_INPUT:
        fprintf(stderr, "METIS_ERROR_INPUT: input data error\n" );
      break;
      case METIS_ERROR_MEMORY:
        fprintf(stderr, "METIS_ERROR_MEMORY: could not allocate memory error\n" );
      break;
      case METIS_ERROR:
        fprintf(stderr, "METIS_ERROR: generic error\n" );
      break;
    }
    _MMG5_SAFE_FREE( xadj );
    _MMG5_SAFE_FREE( adjncy );
    _MMG5_SAFE_FREE( part );
    return (0);
  }

  /* count_per_grp: count elements per group */
  _MMG5_SAFE_CALLOC( countPerGrp, ngrp, int );
  for ( tet = 0; tet < meshOld->ne ; tet++ )
    countPerGrp[part[tet]]++;
for ( i = 0; i < ngrp ; i++ )
  printf( "+++++NIKOS+++++[%d/%d]: group[%d] has %d elements\n", parmesh->myrank+1, parmesh->nprocs, i, countPerGrp[i] );

  /* Create list of subgroups struct */

  _MMG5_SAFE_CALLOC( grpsNew, ngrp, PMMG_Grp );

//NIKOS TODO: do MMG3D_Init_mesh/MMG3D_Set_meshSize offer sth? otherwise allocate on your own
  for ( grpId = 0; grpId < ngrp; ++grpId ) {
    PMMG_pGrp grpCur = &grpsNew[grpId];
    MMG5_pMesh meshCur = NULL;
    grpCur->mesh = NULL;
    grpCur->sol  = NULL;
    grpCur->disp = NULL;
    //_MMG5_SAFE_CALLOC( ptr, size, type );
    //_MMG5_SAFE_REALLOC( ptr, size, type, message );
    MMG3D_Init_mesh( MMG5_ARG_start, MMG5_ARG_ppMesh, &grpCur->mesh,
                     MMG5_ARG_ppMet, &grpCur->sol, MMG5_ARG_end );
    MMG3D_Set_meshSize( grpCur->mesh, countPerGrp[grpId], countPerGrp[grpId], 0, 0, 0, 0 );

    meshCur = grpCur->mesh;
    meshCur->xtmax = meshCur->ne / 2;
    _MMG5_ADD_MEM( meshCur, (meshCur->xtmax + 1) * sizeof(MMG5_xTetra), "boundary tetrahedra",
                   fprintf(stderr,"  Exit program.\n");
                   exit(EXIT_FAILURE) );
    _MMG5_SAFE_CALLOC( meshCur->xtetra, meshCur->xtmax + 1, MMG5_xTetra );

    /* memory to store normals for boundary points */
    meshCur->xpmax  = meshCur->npmax;
    _MMG5_ADD_MEM(meshCur, (meshCur->xpmax + 1) * sizeof(MMG5_xPoint), "boundary points", return(0) );
    _MMG5_SAFE_CALLOC(meshCur->xpoint, meshCur->xpmax + 1, MMG5_xPoint );
  }

//NIKOS TODO: LOOP OVER part ngrp TIMES or USE A tmp[NGROUPS][NP] ARRAY AND LOOP ONLY ONCE? it wastes memory (eg 10 groups x 100k tetra = 4Mb of ints) but only loops over part once
  for ( grpId = 0 ; grpId < ngrp ; grpId++ ) {
    MMG5_pMesh meshCur = grpsNew[grpId].mesh;

    // Will use the MMG5_Point.flag field to assign local numbering in the newly created subgroups.
    //NIKOS TODO: this could be replaced by a vector<bool> flag
    for ( poi = 1; poi < meshOld->np + 1; poi++ )
      meshOld->point[poi].flag = 0;

    tetPerGrp = 0;
    poiPerGrp = 0;
    for ( tet = 1; tet < meshOld->ne + 1; tet++ ) {
      MMG5_pTetra tetraCur = NULL; // pointer to the tetra being processed in the subgroup mesh struct

      /* Skip elements that do not belong in the group processed in this iteration */
      if ( grpId != part[tet-1] )
        continue;

      ++tetPerGrp;
      tetraCur = meshCur->tetra + tetPerGrp;

      /* add tetrahedron to subgroup (copy from original group) */
      memcpy( tetraCur, &meshOld->tetra[tet], sizeof(MMG5_Tetra) );
      tetraCur->base = 0;
      tetraCur->mark = 0;
      tetraCur->flag = 0;
      //NIKOS TODO: there are also cases where we have to update existing boundaries, eg when boundary tetras are also interfaced with new groups
      // Handle xTetras
      if ( tetraCur->xt != 0 ) {
        ++meshCur->xt;
        memcpy( meshCur->xtetra + meshCur->xt, &meshOld->xtetra[ meshOld->tetra[tet].xt], sizeof(MMG5_xTetra) );
        tetraCur->xt = meshCur->xt;
      } else {
        printf ("there may be more: new boundary between subgroups\n");
      }

//printf ( "+++++NIKOS[%d/%d]:: tetra %4d(%4d), mesh xt: %3d\n", grpId+1, ngrp, tet, tetPerGrp, meshOld->tetra[tet].xt);
      /* Add tetrahedron vertices in points struct and adjust tetrahedron vertices indices */
      for ( poi = 0; poi < 4 ; poi++ ) {
        // if it is the first time this point is seen in this subgroup
        if ( 0 == meshOld->point[ meshOld->tetra[tet].v[poi] ].flag ) {

          // Add point in subgroup point array
          ++poiPerGrp;
          memcpy( meshCur->point + poiPerGrp, &meshOld->point[ meshOld->tetra[tet].v[poi] ], sizeof(MMG5_Point) );

          // update tetra vertex reference
          tetraCur->v[poi] = poiPerGrp;

          // "Remember" the assigned subgroup point id
          meshOld->point[ meshOld->tetra[tet].v[poi] ].flag = poiPerGrp;

          // Handle xPoints 
          if ( meshCur->point[poiPerGrp].xp != 0 ) {
            ++meshCur->xp;
            memcpy( meshCur->xpoint + meshCur->xp, &meshOld->xpoint[ meshOld->point[ meshOld->tetra[tet].v[poi] ].xp ], sizeof(MMG5_xPoint) );
            meshCur->point[poiPerGrp].xp = meshCur->xp;
          } else {
            printf( " some other more border cases, think about it \n" );
          }

        // point is already included in this subgroup
        } else {
          // update tetra vertex reference
          tetraCur->v[poi] = meshOld->point[ meshOld->tetra[tet].v[poi] ].flag;
        }
//        printf ( "adja: %5d, point[%4d-%4d-%d].xt: %d \n", meshOld->adja[ 4 * (tet - 1) + 1 + poi ], meshOld->tetra[tet].v[poi], tetPerGrp, poi, meshOld->point[ meshOld->tetra[tet].v[poi] ].xp);
      }
    }
    assert( (meshCur->ne == tetPerGrp) && "Error in PMMG_splitGrps" );
    printf( "+++++NIKOS[%d/%d]:: added %d points in group with %d tetra out of %d tetras expected.\n", grpId+1, ngrp, poiPerGrp, tetPerGrp, meshCur->ne );
  }

  for ( grpId = 0 ; grpId < ngrp ; grpId++ ) {
    MMG5_pMesh meshCur = grpsNew[grpId].mesh;
    //MMG5_saveMshMesh( meshCur, mesMMG5_pSol sol,const char *filename)
    char name[20];
    sprintf(name, "mesh-%02d.mesh", grpId);
    MMG3D_saveMesh( meshCur, name );
  }
  //NIKOS TODO: Handle adja array

  //#error NIKOS: CHANGE THE MEMORY ALLOCATIONS WITH PROPER ALLOCATION+REALLOCATION ??
  //MMG3D_Free_all(MMG5_ARG_start,
  //               MMG5_ARG_ppMesh, &meshOld, MMG5_ARG_ppMet, &grpsOld->met,
  //               MMG5_ARG_end);
  //grpsOld = grpsNew;
  //parmesh->ngrp = ngrp;

  _MMG5_SAFE_FREE( xadj );
  _MMG5_SAFE_FREE( adjncy );
  _MMG5_SAFE_FREE( part );
  _MMG5_SAFE_FREE( countPerGrp );
  return( 1 );
}
