/*******************************************************************************
 Driver program

 COMPILE:
 gcc main.c read_input.c memory.c aux.c sources.c scattering.c \
   scattering_iso.c scattering_rlg.c scattering_hg.c scattering_ff.c \
   scattering_truncate.c reflectance.c bottom.c skyrad.c accumulators.c \
   structures.c geometry.c rotation.c output.c tr_interface.c ray.c \
   intersect.c mc_backward.c statistics.c -lm -lgsl -O3 -o mc_test.o

gcc main.c aux.c mc.c skyrad.c intersect.c statistics.c memory.c geometry.c ray.c sources.c accumulators.c reflectance.c bottom.c read_input.c structures.c rotation.c scattering.c scattering_iso.c scattering_rlg.c scattering_hg.c scattering_ff.c scattering_truncate.c -O3 -lgsl -lgslcblas -lm -o ../mc_solver_free_v1.7.o


*******************************************************************************/

 #include <stdio.h>					// Functions:  fprintf, printf,
 #include <stdlib.h>					// Functions:  calloc
 #include <string.h>					// Functions:  strcmp, strcpy
 #include <time.h>
 #include <gsl/gsl_rng.h>  				// Functions:  gsl_rng_alloc, gsl_rng_set, gsl_rng_uniform_pos
                          				// Data Type:  gsl_rng
 #include <gsl/gsl_spline.h>				// Functions:  gsl_interp_accel_alloc, gsl_spline_alloc, gsl_spline_eva
							// Structures: gsl_interp_accel, gsl_spline
 #include <gsl/gsl_math.h>
 #include <gsl/gsl_integration.h>

 #include "config.h"
 #include "memory.h"
 #include "aux.h"
 #include "constants.h"
 #include "sources.h"

 #include "mc_backward.h"
 #include "skyrad.h"					// Functions:  read_skyrad
// #include "output.h"					// Functions: write_vec, write_grid, print_sim
 #include "statistics.h"				// Functions: calc_se
 #include "intersect.h"
 #include "accumulators.h"
 #include "read_input.h"		// input_read, input_fprintf
 #include "structures.h"
 #include "scattering.h"
 #include "reflectance.h"
 #include "bottom.h"

 int main(int argc, char *argv[])
 {

   // Declaration of variables *************************************************

   // Configuration flags:
   int cnfg_shdw = 0;
   int cnfg_spres = 0;
   #ifdef SHADOWING
   cnfg_shdw = 1;
   #endif // SHADOWING
   #ifdef SPATIALLY_RESOLVED
   cnfg_spres = 1;
   #endif // SPATIALLY_RESOLVED

   // Variables for setup and reporting:
   char version[] = "1.6";				// Program version
   time_t start, end; 					// Timing execution
   int cj, cz;						// Generic counting variables
 
   // Variables controlling general simulation:
   int sim_nr = 0;					// Number of rays to trace, unitless, [SUBDV, Inf)
   int sim_ns = 0;					// Number of Sun zenith angles, unitless, [0, 9]
   double sim_f0 = 0.0;					// External irradiance incident on the medium, W/m^2, (0, Inf)
   double * sim_sza;					// Pointer to sun zenith angles; actual values should be [0, 90], degrees
   double * sim_saa;					// Pointer to sun azimuth angles; actual values should be [0, 360], degrees

   // Variables describing the sky radiance:
   struct skyradiance * skr = skr_alloc();		// Sky radiance distribution parameters;
   int skr_nx = 0;					// Number of azimuthal bins
   int skr_ny = 0;					// Number of zenith bins
   double skr_resx = 0.0;				// Azimuthal angle resolution
   double skr_resy = 0.0;				// Polar angle resolution
   char ** skr_fls;					// Pointer to Sky radiance file paths

   // Variables describing the MC accumulator:
   struct accumulator_bmc * accm_dr_f = accm_b_alloc();	// Accumulator for free diffuse component
   struct accumulator_bmc * accm_df_f = accm_b_alloc();	// Accumulator for free direct component
   struct accumulator_bmc * accm_dr_s = accm_b_alloc();	// Accumulator for shadowed diffuse component
   struct accumulator_bmc * accm_df_s = accm_b_alloc();	// Accumulator for shadowed direct component
   char acc_geom[STRMXLEN];				// Geometry of the accumulator
   int acc_fgeom = 0;					// Flag if acc_geom was requested as 0 (not reported) but SPATIALLY_RESOLVED is defined.
   double acc_ext = 0.0;				// "Radius" of the spatially resolved accumulator
   double acc_resx = 0.0;				// X axis resolution of the spatially resolved accumulator
   double acc_resy = 0.0;				// Y axis resolution of the spatially resolved accumulator

   // Variables describing medium:
   double iop_na = 0.0;					// Real part of the refractive index of air - [1, ?] - unitless
   double iop_nw = 0.0;					// Real part of the refractive index of water relative to air - [1, ?] - unitless
   double iop_c = 0.0;					// Beam attenuation coefficient - [0, Inf) - 1/m
   int iop_nw0 = 1;					// Number of single scattering albedos - [0, Inf) 
   double * iop_w0;					// Pointer to single scattering albedos (actual values should be [0, 1] - unitless)

   // Variables describing the source:
   struct source * src = src_alloc();			// Source parameters;
   char src_tp[STRMXLEN];				// Source type;
   double src_ref_o[3] = {0.0};				// Source reference origin
   double src_rel_o[3] = {0.0};				// Source origin relative to reference
   double src_s[3] = {0.0};				// Spherical directions of the source axis;
   double src_fov = 0.0;				// Field-of-view of the source; 
   double src_stks[STKS_N] = {0.0};			// Stokes parameters of the source;

   // Variables for scattering functions:
   struct scattering * scat = scat_alloc();		// Scattering parameters;
   char scat_tp[STRMXLEN];				// Scattering type - ff, hg, isotropic, petzold
   int scat_trc = 0;					// Logical; should the Phase Function difraction peak be truncated?
   char scat_mtd[STRMXLEN];				// Method to retrieve scattering values: "lut", "itp", "min"
   double scat_depolr;					// Depolarization ratio for Rayleigh scattering
   double scat_g = 0;					// Henyey-Greestein anisotropy parameter - [-1, 1] - unitless
   double scat_fbb = 0.0;				// Backscattering fraction (only used if Phase Function if "ff")

   // Variables describing the bottom:
   struct bottom * btt = btt_alloc();			// Bottom parameters;
   char btt_tp[STRMXLEN];				// Type of bottom reflectance BRDF
   double btt_d = INFINITY;				// Bottom depth - (0, Inf) - meters
   int btt_nbr = 0;					// The number of bottom reflectances
   double * btt_bhr;					// Pointer to bottom hemispherical-directional (or bi-hemispherical) reflectance (actual values should be [0, 1] - unitless)
   double btt_rho; 
   double btt_k;
   double btt_thetaf; 
   double btt_phif;

   // Variables describing shadowing structures:
   int str_def = 0;					// Flag to indicate if structures are defined
   int str_ncl = 0;					// Number of cylinders
   int str_ncn = 0;					// Number of cones
   int str_ncb = 0;					// Number of cuboids
   int expand_str_ncb = 0;				// Logical: expand str_nbx with a encopassing bounding box of all the boxes?
   struct str_cyln ** cylns;				// Pointer to pointers of cylinders
   struct str_cone ** cones;				// Pointer to pointers of cones
   struct str_cubd ** cubds;				// Pointer to pointers of cuboids

   // Read input and setup: ****************************************************
   int ipfnl = 0;
   time(&start);
   if ( argc != 2 )
   {
     printf("Usage: %s path/to/inputfile.txt\n", argv[0]);
     exit(-1);
   }
   if ( strlen(argv[1]) > (STRMXLEN - 1) )
   {
     printf("\nERROR: Input filename longer than %d characters.\n", STRMXLEN - 1);
     exit(-1);
   } else {
     ipfnl = strlen(argv[1]) - (sizeof (char) * 4);
   }


   input_read(argv[1], &sim_nr, &sim_ns, &sim_sza, &sim_saa, &sim_f0, &skr_resx,
     &skr_resy, &skr_nx, &skr_ny, &skr_fls, &iop_na, &iop_nw, &iop_c, &iop_nw0,
     &iop_w0, scat_tp, &scat_trc, scat_mtd, &scat_depolr, &scat_g, &scat_fbb,
     &btt_d, btt_tp, &btt_thetaf, &btt_phif, &btt_rho, &btt_k,
     &btt_nbr, &btt_bhr, src_tp, &src_fov, src_s, src_ref_o, src_rel_o, 
     &acc_fgeom, acc_geom, &acc_ext, &acc_resx, &acc_resy, &str_def, &str_ncl,
     &cylns, &str_ncn, &cones, &str_ncb, &cubds, &expand_str_ncb, cnfg_spres,
     cnfg_shdw);

   skr_setup(skr, sim_ns, skr_ny, skr_nx, skr_resy, skr_resx,
     (char const **) skr_fls);

   src_stks[0] = 1.0;
   src_setup(src, src_fov, src_s, src_ref_o, src_rel_o, src_stks, src_tp);

   scat_setup(scat, scat_tp, scat_depolr, scat_g, scat_fbb, scat_mtd, scat_trc);

   btt_setup(btt, btt_d, btt_tp, btt_nbr, btt_bhr, btt_k);

   input_fprintf(stdout, sim_nr, src_tp, src_fov, src->o, sim_ns, sim_sza, 
     sim_f0, iop_na, iop_nw, iop_c, iop_nw0, iop_w0, scat_tp, scat_g, scat_fbb,
     btt_d, btt_tp, btt_nbr, btt_bhr, acc_fgeom, acc_geom, acc_ext, acc_resx, 
     acc_resy, str_ncl, cylns, str_ncn, cones, str_ncb, cubds, src, scat, btt,
     version);

   accm_setup(accm_df_f, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_f, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);

   #ifdef SHADOWING
   accm_setup(accm_df_s, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_s, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   #endif // SHADOWING

   // Prepare output: **********************************************************
   FILE * fpo;									// Pointer for output file
   char ofn[STRMXLEN];								// Output file name
   char ofbn[STRMXLEN];								// Output base filename
   char sufx[STRMXLEN];								// Output filename suffix

   strncpy (ofbn, argv[1], ipfnl);
   ofbn[ipfnl] = '\0';

   struct accumulator_bmc * accm_df_f_mn = accm_b_alloc();			// Mean of the free diffuse component
   struct accumulator_bmc * accm_dr_f_mn = accm_b_alloc();			// Mean of the free direct component
   struct accumulator_bmc * accm_df_f_se = accm_b_alloc();			// Standard error of the free diffuse component
   struct accumulator_bmc * accm_dr_f_se = accm_b_alloc();			// Standard error of the free direct component

   accm_setup(accm_df_f_mn, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_df_f_se, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_f_mn, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_f_se, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);

   #ifdef SHADOWING
   struct accumulator_bmc * accm_df_s_mn = accm_b_alloc();			// Mean of the shadowed diffuse component
   struct accumulator_bmc * accm_dr_s_mn = accm_b_alloc();			// Mean of the shadowed direct component
   struct accumulator_bmc * accm_df_s_se = accm_b_alloc();			// Standard error of the shadowed diffuse component
   struct accumulator_bmc * accm_dr_s_se = accm_b_alloc();			// Standard error of the shadowed direct component

   accm_setup(accm_df_s_mn, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_df_s_se, acc_geom, sim_ns + 2, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_s_mn, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   accm_setup(accm_dr_s_se, acc_geom, sim_ns, iop_nw0, btt_nbr, acc_ext,
     acc_resy, acc_resx);
   #endif // SHADOWING

   strncpy(ofn, ofbn, STRMXLEN);
   strcat(ofn, "_out_summary.txt");
   fpo = fopen (ofn, "w");
   if ( !fpo )
   {
     printf("\nERROR: Failed to create output file %s\n", ofn);
     exit(-1);
   }
   input_fprintf(fpo, sim_nr, src_tp, src_fov, src->o, sim_ns, sim_sza, 
     sim_f0, iop_na, iop_nw, iop_c, iop_nw0, iop_w0, scat_tp, scat_g, scat_fbb,
     btt_d, btt_tp, btt_nbr, btt_bhr, acc_fgeom, acc_geom, acc_ext, acc_resx, 
     acc_resy, str_ncl, cylns, str_ncn, cones, str_ncb, cubds, src, scat, btt,
     version);

   // Run simulations: *********************************************************

   printf("Progress..00%%");
   fflush(stdout);
   cz = 1;
   for (cj = 0; cj < SUBDV; cj++)
   {
     bmc(sim_nr / SUBDV, sim_ns, iop_nw0, btt_nbr, sim_sza, sim_saa, sim_f0,
       iop_na, iop_nw, iop_c, iop_w0, str_ncl, str_ncn, str_ncb, src, scat,
       btt, skr, (struct str_cyln const **) cylns, 
       (struct str_cone const **) cones,
       (struct str_cubd const **) cubds, 
       accm_df_f, accm_df_s, accm_dr_f, accm_dr_s);

     sprintf(sufx, "_S%02d", cj);
     strcat(sufx, "_dir_f_spi_temp");
     accm_b_write_vec(accm_dr_f, sim_ns, ofbn, sufx, iop_w0, btt_bhr, sim_sza);
     sprintf(sufx, "_S%02d", cj);
     strcat(sufx, "_dif_f_spi_temp");
     accm_b_write_vec(accm_df_f, sim_ns, ofbn, sufx, iop_w0, btt_bhr, sim_sza);

     #ifdef SHADOWING
     sprintf(sufx, "_S%02d", cj);
     strcat(sufx, "_dir_s_spi_temp");
     accm_b_write_vec(accm_dr_s, sim_ns, ofbn, sufx, iop_w0, btt_bhr, sim_sza);
     sprintf(sufx, "_S%02d", cj);
     strcat(sufx, "_dif_s_spi_temp");
     accm_b_write_vec(accm_df_s, sim_ns, ofbn, sufx, iop_w0, btt_bhr, sim_sza);
     #endif // SHADOWING

     accm_b_sum(accm_dr_f_mn, accm_dr_f, 1.0 / SUBDV);
     accm_b_sum(accm_df_f_mn, accm_df_f, 1.0 / SUBDV);
     accm_b_reset(accm_dr_f);
     accm_b_reset(accm_df_f);

     #ifdef SHADOWING
     accm_b_sum(accm_dr_s_mn, accm_dr_s, 1.0 / SUBDV);
     accm_b_sum(accm_df_s_mn, accm_df_s, 1.0 / SUBDV);
     accm_b_reset(accm_dr_s);
     accm_b_reset(accm_df_s);
     #endif // SHADOWING

     if ((cj + 1) / (SUBDV / 10) == cz)
     {
       printf("..%02d%%", (cj + 1) * 10);
       cz++;
     }
     fflush(stdout);
   }
   printf("\n\n");

   /*
   Calculate the standard error:
   For the spatially resolved results, only the average is calculated. It is 
   simple to add the spatially resolved uncertainty too, if needed.
   */

   strncpy(sufx, "_dir_f_spi_temp", STRMXLEN);
   calc_se(accm_dr_f_se, accm_dr_f_mn, btt_bhr, sufx, ofbn);
   strncpy(sufx, "_dif_f_spi_temp", STRMXLEN);
   calc_se(accm_df_f_se, accm_df_f_mn, btt_bhr, sufx, ofbn);

   #ifdef SHADOWING
   strncpy(sufx, "_dir_s_spi_temp", STRMXLEN);
   calc_se(accm_dr_s_se, accm_dr_s_mn, btt_bhr, sufx, ofbn);
   strncpy(sufx, "_dif_s_spi_temp", STRMXLEN);
   calc_se(accm_df_s_se, accm_df_s_mn, btt_bhr, sufx, ofbn);
   #endif // SHADOWING

   // Report average and standard error:
   accm_b_write_vec(accm_dr_f_mn, sim_ns, ofbn, "_out_dir_f_mn_spi", iop_w0, 
     btt_bhr, sim_sza);
   accm_b_write_vec(accm_df_f_mn, sim_ns, ofbn, "_out_dif_f_mn_spi", iop_w0, 
     btt_bhr, sim_sza);
   accm_b_write_vec(accm_dr_f_se, sim_ns, ofbn, "_out_dir_f_se_spi", iop_w0, 
     btt_bhr, sim_sza);
   accm_b_write_vec(accm_df_f_se, sim_ns, ofbn, "_out_dif_f_se_spi", iop_w0, 
     btt_bhr, sim_sza);

   #ifdef SHADOWING
   if ( str_ncl > 0 || str_ncn > 0 || str_ncb > 0 )
   {
     accm_b_write_vec(accm_dr_s_mn, sim_ns, ofbn, "_out_dir_s_mn_spi", iop_w0, 
       btt_bhr, sim_sza);
     accm_b_write_vec(accm_df_s_mn, sim_ns, ofbn, "_out_dif_s_mn_spi", iop_w0, 
       btt_bhr, sim_sza);
     accm_b_write_vec(accm_dr_s_se, sim_ns, ofbn, "_out_dir_s_se_spi", iop_w0, 
       btt_bhr, sim_sza);
     accm_b_write_vec(accm_df_s_se, sim_ns, ofbn, "_out_dif_s_se_spi", iop_w0, 
       btt_bhr, sim_sza);
   }
   #endif // SHADOWING

   #ifdef SPATIALLY_RESOLVED
   if (strcmp (acc_geom, "none") && acc_fgeom == 0)
   {
     accm_b_write_grid(accm_dr_f_mn, sim_ns, ofbn, "_out_dir_f_mn_spr", iop_w0, 
       btt_bhr, sim_sza);
     accm_b_write_grid(accm_df_f_mn, sim_ns, ofbn, "_out_dif_f_mn_spr", iop_w0, 
       btt_bhr, sim_sza);
     #ifdef SHADOWING
     if ( str_ncl > 0 || str_ncn > 0  || str_ncb > 0 )
     {
       accm_b_write_grid(accm_dr_s_mn, sim_ns, ofbn, "_out_dir_s_mn_spr", 
         iop_w0, btt_bhr, sim_sza);
       accm_b_write_grid(accm_df_s_mn, sim_ns, ofbn, "_out_dif_s_mn_spr", 
         iop_w0, btt_bhr, sim_sza);
     }
     #endif // SHADOWING
   }
   #endif // SPATIALLY_RESOLVED

   time(&end);
   double total = (double) (end - start) / 60.0;
   printf("Execution time: %lf min\n\n", total);

   // Deallocate memory: *******************************************************
   free_1d( &sim_sza );
   free_1d( &sim_saa );
   for (int i = 0; i < (sim_ns + 2); i++)
     free( skr_fls[i] );
   free( skr_fls );
   skr_free ( &skr );
   free_1d( &iop_w0 );
   free_1d( &btt_bhr );
   btt_free( &btt );
   src_free ( &src );
   scat_free( &scat );
   accm_b_free( &accm_dr_f );
   accm_b_free( &accm_df_f );
   accm_b_free( &accm_dr_s );
   accm_b_free( &accm_df_s );
   accm_b_free( &accm_dr_f_mn );
   accm_b_free( &accm_dr_f_se );
   accm_b_free( &accm_df_f_mn );
   accm_b_free( &accm_df_f_se );
   #ifdef SHADOWING
   accm_b_free( &accm_dr_s_mn );
   accm_b_free( &accm_dr_s_se );
   accm_b_free( &accm_df_s_mn );
   accm_b_free( &accm_df_s_se );
   #endif // SHADOWING
printf("GOT HERE 04\n");
   if ( cylns )
   {
     for (int i = 0; i < str_ncl; i++)
       str_cyln_free( &cylns[i] );
     free( cylns );
   }
printf("GOT HERE 05\n");
   if ( cones )
   {
     for (int i = 0; i < str_ncn; i++)
       str_cone_free( &cones[i] );
     free( cones );
   }
printf("GOT HERE 06\n");
   if ( cubds )
   {
     for (int i = 0; i < str_ncb; i++)
       str_cubd_free( &cubds[i] );
     free( cubds );   
   }
printf("GOT HERE 07\n");
   return 0;
 }


