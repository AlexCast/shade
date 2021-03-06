
/*******************************************************************************
 structures.c

 Alexandre Castagna Mourão e Lima (alexandre.castagna@ugent.be)
 Version: 1.6
 Date: 2021-03-25
 License: GPL-3.0

 General functions to read, write and setup the internal representation of bi- 
 and three-dimensional geometrical structures. The datatype definitions of those
 structures is in the structures.h header file.

 The functions are:
 str_x_alloc   - Allocate structure;
 str_x_free    - Release memory of structure;
 str_x_read    - Read parameters from input file;
 str_x_setup   - Initialize structure with parameter values;
 str_x_printf  - Formatted detailed print of a structure;
 str_xs_printf - Formatted summary print of an array of structures;

 where "x" is:
 rect - rectangles;
 ellp - ellipses;
 trig - triangles; // Not yet implemented;
 cubd - cuboids;
 cyln - cylinders;
 sphd - spheroids; // Not yet implemented;
 prsm - prisms;    // Not yet implemented;

*******************************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

 #include "config.h"
 #include "constants.h"
 #include "aux.h"
 #include "memory.h"
 #include "geometry.h"
 #include "rotation.h"
 #include "structures.h"

/* str_rect_alloc

 Allocate memory for a rectangle structure (and its components).

 INPUT:
 None.

 OUTPUT:
 Pointer to a rectangle structure.

 */

 str_rect* 
 str_rect_alloc
 (
  void
 )
 {
   str_rect* rect = (str_rect*) malloc( sizeof(str_rect) );
   rect->M = calloc_2d(3, 3, "rect->M in str_rect_alloc (structures.h)");
   rect->alp = 0.0;
   rect->rotate_f = 0;

   return rect;
 }

/* str_rect_free

 Release memory of a rectangle structure.

 INPUT:
 rect - Pointer to a rectangle structure pointer.

 OUTPUT:
 None. Releases memory block and set the rectangle pointer to NULL.

 */

 void 
 str_rect_free
 (
  str_rect **rect
 )
 {
   free_2d( 3, &(*rect)->M );
   free( (*rect) );
   (*rect) = NULL;
 }

/* str_rect_read

 Reads a rectangle from a formatted input file.

 INPUT:
 fi      - File stream pointer;
 fpos    - File stream position pointer;
 origin  - Pointer to array to receive center of rectangle;
 axis    - Pointer to array to receive orientation of the rectangle normal;
 alpha   - Pointer to azimuthal angle around rectangle's normal;
 lengths - Pointer to array to size in each axis.

 OUTPUT:
 None. Updates the values of origin, axis, xrange, yrange and alpha.

 */

 void 
 str_rect_read
 (
   FILE     *fi, 
   long int *fpos,
   double   *origin,
   double   *axis,
   double   *alpha,
   double   *lengths
 )
 {
   fseek(fi, *fpos, SEEK_SET);
   fscanf(fi, "%lf %lf %lf %lf %lf %lf %lf %lf", 
     &origin[0], &origin[1], &origin[2], 
     &axis[0], &axis[1], alpha, 
     &lengths[0], &lengths[1]);
   *fpos = ftell(fi);

   axis[0] *= RAD;
   axis[1] *= RAD;
   *alpha  *= RAD;
   if ( (axis[0] < 0.0) || (axis[0] > K_PI) || 
        (axis[1] < 0.0) || (axis[1] > K_2PI) )
   {
     printf("\nERROR: The orientation axis of the rectangle must be between "
       "0º and 180º for the polar angle and 0º and 360º for the azimuthal "
       "angle.\n\n");
     exit(-1);
   }
   if ( ((*alpha) < 0.0) || ((*alpha) > K_2PI) )
   {
     printf("\nERROR: The azimuthal rotation angle around the normal must be "
       "between 0º and 360º.\n\n");
     exit(-1);
   }
   if ( (lengths[0] < TOLERANCE) || (lengths[1] < TOLERANCE) )
   {
     printf("\nERROR: The X and Y lengths must be greater than 0.\n\n");
     exit(-1);
   }
 }

/* str_rect_setup

 Setup the rectangle parameter structure. The details of the rectangle structure
 are presented in the structures.h header file.

 INPUT:
 rect    - Pointer to the ellipse structure;
 origin  - Pointer to array to receive center of rectangle;
 axis    - Pointer to array to receive orientation of the rectangle normal;
 alpha   - Pointer to azimuthal angle around the rectangle's normal;
 lengths - Pointer to array to receive the sizes in each axis.

 OUTPUT:
 None. Updates the rectangle structure pointed by rect.

 */

 void 
 str_rect_setup
 (
   str_rect     *rect,
   double const *origin,
   double const *axis,
   double const alpha,
   double const *lengths
 )
 {
   for (int i = 0; i < 2; i++)
   {
     rect->o[i]  = origin[i];
     rect->s[i]  = axis[i];
     rect->hl[i] = 0.5 * lengths[i];
   }
   rect->o[2] = origin[2];
   rect->s[2] = 1.0;
   rect->alp  = alpha;
   rect->x[0] = -lengths[0] * 0.5;
   rect->x[1] =  lengths[0] * 0.5;
   rect->y[0] = -lengths[1] * 0.5;
   rect->y[1] =  lengths[1] * 0.5;

   if ( (axis[0] < TOLERANCE) && (axis[1] < TOLERANCE) && (alpha < TOLERANCE) )
   {
     rect->u[0] = 0.0;
     rect->u[1] = 0.0;
     rect->u[2] = 1.0;
     rect->M[0][0] = 1.0;
     rect->M[1][1] = 1.0;
     rect->M[2][2] = 1.0;
     rect->rotate_f = 0;
   }
   else
   {
     sph_to_cos_unit(rect->u, rect->s);
     double **R = calloc_2d(3, 3, "R in str_rect_setup (structures.c)");
     rot_mat_ZYZ (R, rect->u, rect->s, alpha);
     mat_transpose(rect->M, (double const **) R, 3, 3);
     free_2d(3, &R);
     rect->rotate_f = 1;
   }
 }

/* str_rect_printf, str_rects_printf

 Print detailed information of a rectangle or a summary of multiple rectangles.

 INPUT:
 rect - Pointer to the rectangle structure;

 OUTPUT:
 None. Prints the rectangle parameters.

 */

 void
 str_rect_fprintf 
 (
   FILE *odv,
   str_rect const *rect,
   int const indent 
 )
 {
   char pre_0[STRMXLEN] = "";
   char pre_1[STRMXLEN] = "";
   for (int i = 0; i < indent; i++)
     strcat(pre_0, "  ");
   strncpy(pre_1, pre_0, STRMXLEN);
   strcat(pre_1, "  ");

   fprintf(odv, "%sRectangle parameters:\n", pre_0);
   fprintf(odv, "%sOrigin:   % .2e, % .2e, % .2e (m)\n", pre_1, 
     rect->o[0], 
     rect->o[1], 
     rect->o[2]);
   fprintf(odv, "%sX-range:  % .2e, % .2e  (%.2e m)\n", pre_1,
     rect->x[0], 
     rect->x[1], 
     2.0 * rect->hl[0]);
   fprintf(odv, "%sY-range:  % .2e, % .2e  (%.2e m)\n", pre_1, 
     rect->y[0],
     rect->y[1], 
     2.0 * rect->hl[1]);
   fprintf(odv, "%sAxis:      %6.2lfº, %6.2lfº (% .2e, % .2e, % .2e)\n", pre_1, 
     rect->s[0] * DEG, 
     rect->s[1] * DEG, 
     rect->u[0], 
     rect->u[1],
     rect->u[2]);
   fprintf(odv, "%sAlpha:     %6.2lfº\n", pre_1, rect->alp * DEG);
   fprintf(odv, "%sRotate:    %d\n", pre_1, rect->rotate_f);
   mat_fprintf(odv, (double const **) rect->M, 3, 3, 1.0, "Rotation matrix", indent+1);
 }

 void
 str_rects_printf
 (
  str_rect const **rects,
  int n
 )
 {
   printf("  Number of rectangles: %d\n", n);
   printf("           type                     origin (m)       axis (º) "
     " alpha (º)           Length (m)\n");
   for(int i = 0; i < n; i++)
   {
     printf("  %02d  %s  % .2e,% .2e,% .2e  %6.2lf,%6.2lf     %6.2lf  "
       "% .2e,% .2e\n",
       i+1,
       ( ABS(rects[i]->hl[0] - rects[i]->hl[1]) < TOLERANCE ) ? 
         "   Square" : "Rectangle",
       rects[i]->o[0], 
       rects[i]->o[1], 
       rects[i]->o[2], 
       rects[i]->s[0] * DEG, 
       rects[i]->s[1] * DEG, 
       rects[i]->alp * DEG,
       2.0 * rects[i]->hl[0],
       2.0 * rects[i]->hl[1]);
   }
 }

/* str_ellp_alloc

 Allocate memory for an ellipse structure (and its components).

 INPUT:
 None.

 OUTPUT:
 Pointer to an ellipse structure.

 */

 str_ellp* 
 str_ellp_alloc
 (
  void
 )
 {
   str_ellp* ellp = (str_ellp*) malloc( sizeof(str_ellp) );
   ellp->M = calloc_2d(3, 3, "ellp->M in str_ellp_alloc (structures.h)");
   ellp->alp = 0.0;
   ellp->rotate_f = 0;

   return ellp;
 }

/* str_ellp_free

 Release memory of an ellipse structure.

 INPUT:
 ellp - Pointer to an ellipse structure pointer.

 OUTPUT:
 None. Releases memory block and set the ellipse pointer to NULL.

 */

 void 
 str_ellp_free
 (
  str_ellp **ellp
 )
 {
   free_2d(3, &(*ellp)->M);
   free(*ellp);
   *ellp = NULL;
 }

/* str_ellp_read

 Reads an ellipse from a formatted input file.

 INPUT:
 fi     - File stream pointer;
 fpos   - File stream position pointer;
 origin - Pointer to array to receive center of the ellipse;
 axis   - Pointer to array to receive orientation of the ellipse's normal;
 alpha  - Pointer to azimuthal angle around the ellipse's normal;
 radius - Pointer to array to receive the X and Y radius of the ellipse.

 OUTPUT:
 None. Updates the values of origin, axis, radius and alpha.

 */

 void
 str_ellp_read
 (
   FILE     *fi, 
   long int *fpos,
   double   *origin,
   double   *axis,
   double   *alpha,
   double   *radius
 )
 {
   fseek(fi, *fpos, SEEK_SET);
   fscanf(fi, "%lf %lf %lf %lf %lf %lf %lf %lf", 
     &origin[0], &origin[1], &origin[2], 
     &axis[0], &axis[1], alpha,
     &radius[0], &radius[1]);
   *fpos = ftell(fi);

   axis[0] *= RAD;
   axis[1] *= RAD;
   *alpha  *= RAD;

   if ( (axis[0] < 0.0) || (axis[0] > K_PI) || 
        (axis[1] < 0.0) || (axis[1] > K_2PI) )
   {
     printf("\nERROR: The orientation axis of the ellipse must be between "
       "0º and 180º for the polar angle and 0º and 360º for the azimuthal "
       "angle.\n\n");
     exit(-1);
   }
   if ( ((*alpha) < 0.0) || ((*alpha) > K_2PI) )
   {
     printf("\nERROR: The azimuthal rotation angle around the normal must be "
       "between 0º and 360º.\n\n");
     exit(-1);
   }
   if ( radius[0] <= 0.0 || radius[1] <= 0.0 )
   {
     printf("\nERROR: Ellipse's X and Y axis radius must be greater than "
       "zero.\n\n");
     exit(-1);
   }
 }

/* str_ellp_setup

 Setup the ellipse parameter structure. The details of the ellipse structure
 are presented in the structures.h header file.

 INPUT:
 ellp   - Pointer to the ellipse structure;
 origin - Pointer to the origin (center) of the ellipse;
 axis   - Pointer to the orientation axis of the normal;
 alpha  - Pointer to azimuthal angle around ellipse's normal;
 radius - Pointer to radius of the ellipse for X and Y axis, (on its own 
          reference frame);

 OUTPUT:
 None. Updates the ellipse structure pointed by ellp.

 */

 void
 str_ellp_setup
 (
   str_ellp *ellp,
   double const *origin,
   double const *axis,
   double const alpha,
   double const *radius
 )
 {
   for (int i = 0; i < 2; i++)
   {
     ellp->o[i] = origin[i];
     ellp->s[i] = axis[i];
     ellp->r[i] = radius[i];
     ellp->rsq_inv[i] = 1.0 / (radius[i] * radius[i]);
   }
   ellp->o[2] = origin[2];
   ellp->s[2] = 1.0;
   ellp->alp  = alpha;

   if ( (axis[0] < TOLERANCE) && (axis[1] < TOLERANCE) && (alpha < TOLERANCE) )
   {
     ellp->u[0] = 0.0;
     ellp->u[1] = 0.0;
     ellp->u[2] = 1.0;
     ellp->M[0][0] = 1.0;
     ellp->M[1][1] = 1.0;
     ellp->M[2][2] = 1.0;
     ellp->rotate_f = 0;
   }
   else
   {
     sph_to_cos_unit(ellp->u, ellp->s);
     double **R = calloc_2d(3, 3, "R in str_ellp_setup (structures.c)");
     rot_mat_ZYZ (R, ellp->u, ellp->s, alpha);
     mat_transpose(ellp->M, (double const **) R, 3, 3);
     free_2d(3, &R);
     ellp->rotate_f = 1;
   }
 }

/* str_ellp_printf, str_ellp_printf

 Print detailed information of an ellipse or a summary of multiple ellipses.

 INPUT:
 ellp - Pointer to the ellipse structure;

 OUTPUT:
 None. Prints the ellipse parameters.

 */

 void
 str_ellp_fprintf 
 (
   FILE *odv,
   str_ellp const *ellp,
   int const indent 
 )
 {
   char pre_0[STRMXLEN] = "";
   char pre_1[STRMXLEN] = "";
   for (int i = 0; i < indent; i++)
     strcat(pre_0, "  ");
   strncpy(pre_1, pre_0, STRMXLEN);
   strcat(pre_1, "  ");

   fprintf(odv, "%sEllipse parameters:\n", pre_0);
   fprintf(odv, "%sOrigin:   % .2e, % .2e, % .2e (m)\n", pre_1, 
     ellp->o[0], 
     ellp->o[1], 
     ellp->o[2]);
   fprintf(odv, "%sRadius:  % .2e, % .2e  (m)\n", pre_1,
     ellp->r[0], 
     ellp->r[1]);
   fprintf(odv, "%sAxis:      %6.2lfº, %6.2lfº (% .2e, % .2e, % .2e)\n", pre_1, 
     ellp->s[0] * DEG, 
     ellp->s[1] * DEG, 
     ellp->u[0], 
     ellp->u[1],
     ellp->u[2]);
   fprintf(odv, "%sAlpha:     %6.2lfº\n", pre_1, ellp->alp * DEG);
   fprintf(odv, "%sRotate:    %d\n", pre_1, ellp->rotate_f);
   mat_fprintf(odv, (double const **) ellp->M, 3, 3, 1.0, "Rotation matrix", indent+1);
 }

 void
 str_ellps_printf
 (
  str_ellp const **ellps,
  int n
 )
 {
   printf("  Number of ellipses: %d\n", n);
   printf("           type                     origin (m)       axis (º) "
     " alpha (º)           Radius (m)\n");
   for(int i = 0; i < n; i++)
   {
     printf("  %02d  %s  % .2e,% .2e,% .2e  %6.2lf,%6.2lf     %6.2lf  "
       "% .2e,% .2e\n",
       i+1,
       ( ABS(ellps[i]->r[0] -  ellps[i]->r[1]) < TOLERANCE ) ? 
         "   Circle" : "  Ellipse",
       ellps[i]->o[0], 
       ellps[i]->o[1], 
       ellps[i]->o[2], 
       ellps[i]->s[0] * DEG, 
       ellps[i]->s[1] * DEG, 
       ellps[i]->alp  * DEG,
       ellps[i]->r[0],
       ellps[i]->r[1]);
   }
 }



