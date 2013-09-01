/* 
 * -- High Performance Computing Linpack Benchmark (HPL)                
 *    HPL - 2.0 - September 10, 2008                          
 *    Antoine P. Petitet                                                
 *    University of Tennessee, Knoxville                                
 *    Innovative Computing Laboratory                                 
 *    (C) Copyright 2000-2008 All Rights Reserved                       
 *                                                                      
 * -- Copyright notice and Licensing terms:                             
 *                                                                      
 * Redistribution  and  use in  source and binary forms, with or without
 * modification, are  permitted provided  that the following  conditions
 * are met:                                                             
 *                                                                      
 * 1. Redistributions  of  source  code  must retain the above copyright
 * notice, this list of conditions and the following disclaimer.        
 *                                                                      
 * 2. Redistributions in binary form must reproduce  the above copyright
 * notice, this list of conditions,  and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. 
 *                                                                      
 * 3. All  advertising  materials  mentioning  features  or  use of this
 * software must display the following acknowledgement:                 
 * This  product  includes  software  developed  at  the  University  of
 * Tennessee, Knoxville, Innovative Computing Laboratory.             
 *                                                                      
 * 4. The name of the  University,  the name of the  Laboratory,  or the
 * names  of  its  contributors  may  not  be used to endorse or promote
 * products  derived   from   this  software  without  specific  written
 * permission.                                                          
 *                                                                      
 * -- Disclaimer:                                                       
 *                                                                      
 * THIS  SOFTWARE  IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,  INCLUDING,  BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OR  CONTRIBUTORS  BE  LIABLE FOR ANY  DIRECT,  INDIRECT,  INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES  (INCLUDING,  BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT,  STRICT LIABILITY,  OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 * ---------------------------------------------------------------------
 */ 
/*
 * Include files
 */
#include "hpl.h"

#ifdef STDC_HEADERS
void HPL_pdgesv0
(
   HPL_T_grid *                     GRID,
   HPL_T_palg *                     ALGO,
   HPL_T_pmat *                     A
)
#else
void HPL_pdgesv0
( GRID, ALGO, A )
   HPL_T_grid *                     GRID;
   HPL_T_palg *                     ALGO;
   HPL_T_pmat *                     A;
#endif
{
/* 
 * Purpose
 * =======
 *
 * HPL_pdgesv0 factors a N+1-by-N matrix using LU factorization with row
 * partial pivoting.  The main algorithm  is the "right looking" variant
 * without look-ahead. The lower triangular factor is left unpivoted and
 * the pivots are not returned. The right hand side is the N+1 column of
 * the coefficient matrix.
 *
 * Arguments
 * =========
 *
 * GRID    (local input)                 HPL_T_grid *
 *         On entry,  GRID  points  to the data structure containing the
 *         process grid information.
 *
 * ALGO    (global input)                HPL_T_palg *
 *         On entry,  ALGO  points to  the data structure containing the
 *         algorithmic parameters.
 *
 * A       (local input/output)          HPL_T_pmat *
 *         On entry, A points to the data structure containing the local
 *         array information.
 *
 * ---------------------------------------------------------------------
 */ 
/*
 * .. Local Variables ..
 */
   HPL_T_panel                * * panel = NULL;
   HPL_T_UPD_FUN              HPL_pdupdate;
   int                        N, j, jb, n, nb, tag=MSGID_BEGIN_FACT,
                              test=HPL_KEEP_TESTING;
   int ii, jj;
/* ..
 * .. Executable Statements ..
 */
   if( ( N = A->n ) <= 0 ) return;

   HPL_pdupdate = ALGO->upfun; nb = A->nb;
/*
 * Allocate a panel list of length 1 - Allocate panel[0] resources
 */
   panel = (HPL_T_panel **)malloc( sizeof( HPL_T_panel * ) );
   if( panel == NULL )
   { HPL_pabort( __LINE__, "HPL_pdgesv0", "Memory allocation failed" ); }

   HPL_pdpanel_new( GRID, ALGO, N, N+1, Mmin( N, nb ), A, 0, 0, tag,
                    &panel[0] );
/*
 * Loop over the columns of A
 */
   for( j = 0; j < N; j += nb )
   {
       int prc;
       Mindxg2p(j, nb, nb, prc, 0, GRID->nprow);
       if (prc != GRID->nprow-1)
       {

      n = N - j; jb = Mmin( n, nb );
/*
 * Release panel resources - re-initialize panel data structure
 */
      (void) HPL_pdpanel_free( panel[0] );
      HPL_pdpanel_init( GRID, ALGO, n, n+1, jb, A, j, j, tag, panel[0] );
/*
 * Factor and broadcast current panel - update
 */
      HPL_pdfact(               panel[0] );
      //FT_print_matrix_2(GRID, N, N+1, nb, A->A, A->ld);
      (void) HPL_binit(         panel[0] );
      do
      { (void) HPL_bcast(       panel[0], &test ); }
      while( test != HPL_SUCCESS );
      (void) HPL_bwait(         panel[0] );
      //FT_print_matrix_2(GRID, N, N+1, nb, A->A, A->ld);
      HPL_pdupdate( NULL, NULL, panel[0], -1 );

      for (ii = 0; ii < GRID->nprow; ii++)
          for (jj = 0; jj < GRID->npcol; jj++)
                FT_recover(GRID, A, ii, jj, j);
/*
 * Update message id for next factorization
 */
      tag = MNxtMgid( tag, MSGID_BEGIN_FACT, MSGID_END_FACT );

      //FT_print_matrix_2(GRID, N, N+1, nb, A->A, A->ld);
      //printf("***\n");


       }
   }
/*
 * Release panel resources and panel list
 */
   (void) HPL_pdpanel_disp( &panel[0] );

   if( panel ) free( panel );
/*
 * End of HPL_pdgesv0
 */
}


#ifdef STDC_HEADERS
void FT_recover
(
   HPL_T_grid *                     GRID,
   HPL_T_pmat *                     A,
   int                              fail_row,
   int                              fail_col,
   int                              icurr
)
#else
void FT_recover
( GRID, A, fail_row, fail_col, icurr )
   HPL_T_grid *                     GRID;
   HPL_T_pmat *                     A;
   int                              fail_row;
   int                              fail_col;
   int                              icurr;
#endif
{
    if (GRID->myrow == fail_row)
    {
        int myrow, mycol, nprow, npcol;
        myrow = GRID->myrow; mycol = GRID->mycol;
        nprow = GRID->nprow; npcol = GRID->npcol;
        double *red;
        int i, j, m, n, nb;
        m = A->mp; n = A->nq; nb = A->nb;
        red = (double*)malloc(sizeof(double)*m*n);

        if (mycol == fail_col)
            for (i = 0; i < m*n; i++)
                red[i] = 0;
        else if (mycol != npcol-1)
        {
            for (i = 0; i < m; i++)
                for (j = 0; j < n; j++)
                    red[j*m+i] = A->A[j*A->ld+i];
        }
        else 
            for (i = 0; i < m; i++)
                for (j = 0; j < n; j++)
                    red[j*m+i] = -A->A[j*A->ld+i];

        HPL_reduce(red, m*n, HPL_DOUBLE, HPL_sum, fail_col, GRID->row_comm);


        if (mycol == fail_col)
        {
            printf("failed processor %d %d (recovered)\n", myrow, mycol);
            for (i = 0; i < m; i++)
            {
                for (j = 0; j < n; j++)
                {
                    int g_row, g_col;
                    Mindxl2g(g_row, i, nb, nb, myrow, 0, nprow);
                    Mindxl2g(g_col, j, nb, nb, mycol, 0, npcol);
                    if (g_row >= icurr && g_col <= icurr ||
                          g_row <= icurr && g_col <= icurr && g_row >= g_col ||
                          g_row >= icurr && g_col >= icurr && g_row >= g_col) 
                        printf(" x%7.3f ", red[j*m+i]);
                    else 
                    {
                        printf("  %7.3f ", -red[j*m+i]);
                    }
                }
                printf("%d %d\n", myrow, mycol);
            }
            printf("failed processor %d %d (original)\n", myrow, mycol);
            for (i = 0; i < m; i++)
            {
                for (j = 0; j < n; j++) {
                    printf(" %7.3f ", A->A[j*A->ld+i]);
                }
                printf("%d %d\n", myrow, mycol);
            }

        }
    }
}
