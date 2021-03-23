
 #ifndef READINPUT
 #define READINPUT

 #include <stdio.h>		// FILE

 #include "sources.h"		// source
 #include "scattering.h"	// scattering
 #include "bottom.h"		// bottom
 #include "structures.h"	// str_cyln, str_cone, str_cubd
 
 void
 input_read
 (
   const char * flnm,
   int * sim_nr,
   int * sim_ns,
   double ** sim_sza,
   double ** sim_saa,
   double * sim_f0,
   double * skr_resx,
   double * skr_resy,
   int * skr_nx,
   int * skr_ny,
   char *** skr_fls,
   double * iop_na,
   double * iop_nw,
   double * iop_c,
   int * iop_nw0,
   double ** iop_w0,
   char * scat_tp,
   int * scat_trc,
   char * scat_mtd,
   double * scat_d,
   double * scat_g,
   double * scat_fbb,
   double * btt_d,
   char * btt_tp,
   double * btt_par_thetaf,
   double * btt_par_phif, 
   double * btt_par_rho,
   double * btt_par_k,
   int * btt_nbr,
   double ** btt_bhr,
   char * src_tp,
   double * src_fov,
   double * src_saxs,
   double * src_ref_o,
   double * src_rel_o,
   int * acc_fgeom,
   char * acc_geom,
   double * acc_ext,
   double * acc_resx,
   double * acc_resy,
   int * str_def,
   int * str_ncl,
   struct str_cyln *** cylns,
   int * str_ncn,
   struct str_cone *** cones,
   int * str_ncb,
   struct str_cubd *** cubds,
   int * expand_str_ncb,
   int const cnfg_spres,
   int const cnfg_shdw
 );

 void
 input_fprintf
 (
   FILE * fo,
   int const sim_nr,
   char const * src_tp,
   double const src_fov,
   double const * src_pos,
   int const sim_ns,
   double const * sim_sza,
   double const sim_f0,
   double const iop_na,
   double const iop_nw,
   double const iop_c,
   int const iop_nw0,
   double const * iop_w0,
   char const * scat_tp,
   double const scat_g,
   double const scat_fbb,
   double const btt_d,
   char const * btt_tp,
   int const btt_nbr,
   double const * btt_bhr,
   int const acc_fgeom,
   char const * acc_geom,
   double const acc_ext,
   double const acc_resx,
   double const acc_resy,
   int const str_ncl,
   struct str_cyln ** cylns,
   int const str_ncn,
   struct str_cone ** cones,
   int const str_ncb,
   struct str_cubd ** cubds,
   struct source const * src,
   struct scattering const * scat,
   struct bottom const * btt,
   char const * version
 );

 #endif // READINPUT

