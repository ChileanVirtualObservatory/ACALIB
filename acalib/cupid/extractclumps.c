#include "sae_par.h"
#include "mers.h"
#include "ndf.h"
#include "star/ndg.h"
#include "star/cvg.h"
#include "ast.h"
#include "star/kaplibs.h"
#include "star/irq.h"
#include "star/grp.h"
#include "star/hds.h"
#include "par.h"
#include "prm_par.h"
#include "cupid.h"
#include <math.h>
#include <string.h>
#include <stdio.h>


void extractclumps( int *status ) {
/*
*+
*  Name:
*     EXTRACTCLUMPS

*  Purpose:
*     Extract previously identified clumps of emission from an NDF.

*  Language:
*     C

*  Type of Module:
*     ADAM A-task

*  Description:
*     This application extract previously identified clumps of emission
*     from a 1, 2 or 3 dimensional NDF. Usually, FINDCLUMPS will first be
*     used to identify the clumps within a given array, and then
*     EXTRACTCLUMPS can be used to find the parameters of the same clumps
*     in a second array.
*
*     Two input NDFs are supplied; the NDF associated with parameter DATA
*     contains the physical data values from which the clumps are to be
*     extracted, whilst the NDF associated with parameter MASK contains
*     integer values that identify the clump to which each pixel belongs. The
*     two NDFs are assumed to be aligned in PIXEL coordinates. An output NDF
*     is created that is a copy of the MASK NDF. Parameters describing the
*     clumps extracted from the DATA NDF are stored in the CUPID extension
*     of the output NDF, and may also be stored in an output catalogue.
*     These are in the same form as the clump parameters created by the
*     FINDCLUMPS command.

*  Usage:
*     extractclumps mask data out outcat

*  ADAM Parameters:
*     BACKOFF = _LOGICAL (Read)
*        If TRUE, the background level in each clump is removed from the
*        clump data values before calculating the reported clump sizes and
*        centroid position. This means that the clump sizes and centroid
*        position will be independent of the background level. The
*        background level used is the minimum data value in the clump. If
*        FALSE, the full data values, including background, are used when
*        calculating the clump sizes and centroid position. This means that
*        clumps on larger backgrounds will be reported as wider than similar
*        clumps on smaller backgrounds. Using BACKOFF=FALSE allows the
*        comparison of clump properties generated by EXTRACTCLUMPS with those
*        generated by the IDL version of CLUMPFIND (see also the ClumpFind
*        configuration parameter "IDLAlg"). Note, the other reported
*        clump properties such as total data value, peak data value, etc,
*        are always based on the full clump data values, including
*        background. [TRUE]
*     FWHMBEAM = _REAL (Read)
*        The FWHM of the instrument beam, in pixels. If DECONV is TRUE, the
*        clump widths written to the output catalogue are reduced (in
*        quadrature) by this amount. The default value is the value stored
*        in the CONFIG component of the CUPID extension in the mask NDF, or
*        2.0 if the CUPID extension does not contain a CONFIG component. []
*     DATA = NDF (Read)
*        The input NDF containing the physical data values.
*     DECONV = _LOGICAL (Read)
*        Determines if the clump properties stored in the output catalogue
*        and NDF extension should be corrected to remove the effect of the
*        instrumental beam width specified by the FWHMBEAM and VELORES
*        parameters. If TRUE, the clump sizes will be reduced and the peak
*        values increased to take account of the smoothing introduced by the
*        beam width. If FALSE, the undeconvolved values are stored in the
*        output catalogue and NDF. Note, the filter to remove clumps smaller
*        than the beam width is still applied, even if DECONV is FALSE. [TRUE]
*     JSACAT = NDF (Read)
*        An optional JSA-style output catalogue in which to store the clump
*        parameters (for KAPPA-style catalogues see parameter "OUTCAT"). No
*        catalogue will be produced if a null (!) value is supplied.
*        The created file will be a FITS file containing a binary table.
*        The columns in this catalogue will be the same as those created
*        by the "OUTCAT" parameter, but the table will in also hold the
*        contents of the FITS extension of the input NDF, and CADC-style
*        provenance headers. [!]
*     LOGFILE = LITERAL (Read)
*        The name of a text log file to create. If a null (!) value is
*        supplied, no log file is created. [!]
*     MASK = NDF (Read)
*        The input NDF containing the pixel assignments. This will
*        usually have been created by the FINDCLUMPS command.
*     OUT = NDF (Write)
*        The output NDF.
*     OUTCAT = FILENAME (Write)
*        An optional KAPPA-style output catalogue in which to store the clump
*        parameters (for JSA-style catalogues see parameter "JSACAT"). See the
*        description of the OUTCAT parameter for the FINDCLUMPS command for
*        further information.
*     SHAPE = LITERAL (Read)
*        Specifies the shape that should be used to describe the spatial
*        coverage of each clump in the output catalogue. It can be set to
*        "None", "Polygon" or "Ellipse". If it is set to "None", the
*        spatial shape of each clump is not recorded in the output
*        catalogue. Otherwise, the catalogue will have an extra column
*        named "Shape" holding an STC-S description of the spatial coverage
*        of each clump. "STC-S" is a textual format developed by the IVOA
*        for describing regions within a WCS - see
*        http://www.ivoa.net/Documents/latest/STC-S.html for details.
*        These STC-S desriptions can be displayed by the KAPPA:LISTSHOW
*        command, or using GAIA. Since STC-S cannot describe regions within
*        a pixel array, it is necessary to set parameter WCSPAR to TRUE if
*        using this option. An error will be reported if WCSPAR is FALSE. An
*        error will also be reported if the WCS in the input data does not
*        contain a pair of scelestial sky axes.
*
*        - Polygon: Each polygon will have, at most, 15 vertices. If the data
*        is 2-dimensional, the polygon is a fit to the clump's outer boundary
*        (the region containing all godo data values). If the data is
*        3-dimensional, the spatial footprint of each clump is determined
*        by rejecting the least significant 10% of spatial pixels, where
*        "significance" is measured by the number of spectral channels that
*        contribute to the spatial pixel. The polygon is then a fit to
*        the outer boundary of the remaining spatial pixels.
*
*        - Ellipse: All data values in the clump are projected onto the
*        spatial plane and "size" of the collapsed clump at four different
*        position angles - all separated by 45 degrees - is found (see the
*        OUTCAT parameter for a description of clump "size"). The ellipse
*        that generates the same sizes at the four position angles is then
*        found and used as the clump shape.
*
*        In general, "Ellipse" will outline the brighter, inner regions
*        of each clump, and "Polygon" will include the fainter outer
*        regions. The dynamic default is "Polygon" if a JSA-style
*        catalogue (see parameters JSACAT) is being created, and "None"
*        otherwise. Note, if a JSA-style catalogue is neing created an
*        error will be reported if "Ellipse" or "None" is selected. []
*     VELORES = _REAL (Read)
*        The velocity resolution of the instrument, in channels. If DECONV is
*        TRUE, the velocity width of each clump written to the output
*        catalogue is reduced (in quadrature) by this amount. The default
*        value is the value stored in the CONFIG component of the CUPID
*        extension in the mask NDF, or 2.0 if the CUPID extension does not
*        contain a CONFIG component. []
*     WCSPAR = _LOGICAL (Read)
*        If a TRUE value is supplied, then the clump parameters stored in
*        the output catalogue and in the CUPID extension of the output NDF,
*        are stored in WCS units, as defined by the current coordinate frame
*        in the WCS component of the input NDF (this can be inspected using
*        the KAPPA:WCSFRAME command). For instance, if the current
*        coordinate system in the input NDF is (RA,Dec,freq), then the
*        catalogue columns that hold the clump peak and centroid positions
*        will use this same coordinate system. The spatial clump sizes
*        will be stored in arc-seconds, and the spectral clump size will
*        be stored in the unit of frequency used by the NDF (Hz, GHz, etc).
*        If a FALSE value is supplied for this parameter, the clump
*        parameters are stored in units of pixels within the pixel coordinate
*        system of the input NDF. The dynamic default for this parameter is
*        TRUE if the current coordinate system in the input NDF represents
*        celestial longitude and latitude in some system, plus a recogonised
*        spectral axis (if the input NDF is 3D). Otherwise, the dynamic
*        default is FALSE. []

*  Synopsis:
*     void extractclumps( int *status );

*  Copyright:
*     Copyright (C) 2006 Particle Physics & Astronomy Research Council.
*     Copyright (C) 2008,2013 Science & Technology Facilities Council.
*     Copyright (C) 2009 University of British Columbia.
*     All Rights Reserved.

*  Licence:
*     This program is free software; you can redistribute it and/or
*     modify it under the terms of the GNU General Public License as
*     published by the Free Software Foundation; either version 2 of
*     the License, or (at your option) any later version.
*
*     This program is distributed in the hope that it will be
*     useful, but WITHOUT ANY WARRANTY; without even the implied
*     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
*     PURPOSE. See the GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 51 Franklin Street,Fifth Floor, Boston, MA
*     02110-1301, USA

*  Authors:
*     DSB: David S. Berry (JAC, Hawaii)
*     EC: Ed Chapin (UBC)
*     {enter_new_authors_here}

*  History:
*     6-APR-2006 (DSB):
*        Original version.
*     11-DEC-2006 (DSB):
*        Added parameter DECONV.
*     29-JAN-2006 (DSB):
*        - Added parameters WCSPAR and LOGFILE.
*        - Store data units in output catalogue.
*     18-MAR-2008 (DSB):
*        Added adam parameter BACKOFF.
*     15-JAN-2009 (DSB):
*        Remove ILEVEL arguments.
*     25-AUG-2009 (EC):
*        Add star/irq.h include as it is no longer in star/kaplibs.h.
*     19-NOV-2013 (DSB):
*        Add parameter JSACAT.
*     15-OCT-2014 (SFG):
*        Add paramter SHAPE.
*     {enter_further_changes_here}

*  Bugs:
*     {note_any_bugs_here}

*-
*/

/* Local Variables: */
   AstFrameSet *iwcs;           /* Pointer to the WCS FrameSet */
   AstKeyMap *config;           /* Pointer to KeyMap holding used config settings */
   AstMapping *map;             /* Pointer to PIXEl->WCS Mapping */
   AstMapping *tmap;            /* Pointer to temporary Mapping */
   AstObject *mconfig;          /* Pointer to KeyMap holding algorithm settings */
   FILE *logfile = NULL;        /* File identifier for output log file */
   Grp *grp;                    /* GRP group holding NDF names */
   HDSLoc *ndfs;                /* Array of NDFs, one for each clump */
   HDSLoc *xloc;                /* HDS locator for CUPID extension */
   IRQLocs *qlocs;              /* HDS locators for quality name information */
   char attr[ 30 ];             /* AST attribute name */
   char buffer[ GRP__SZNAM ];    /* Buffer for GRP element */
   char dataunits[ 21 ];        /* NDF data units */
   char dtype[ 20 ];            /* NDF data type */
   char itype[ 20 ];            /* NDF data type */
   char logfilename[ GRP__SZNAM + 1 ]; /* Log file name */
   char shape[ 10 ];            /* Shape for spatial STC-S regions */
   const char *dom;             /* Axis domain */
   const char *method;          /* Algorithm string supplied by user */
   double beamcorr[ 3 ];        /* Beam width corrections */
   double fb;                   /* FWHMBEAM value */
   double vr;                   /* VELORES value */
   fitsfile *fptr;              /* Pointer to FITS file structure */
   float *rmask;                /* Pointer to cump mask array */
   int *clbnd;                  /* Lower GRID bounds of each clump */
   int *cubnd;                  /* Upper GRID bounds of each clump */
   int *ipa;                    /* Pointer to pixel assignment array */
   int *pa;                     /* Pointer to next pixel assignment value */
   int *pid;                    /* Pointer to next clump ID */
   int backoff;                 /* Remove background when finding clump sizes? */
   int blockf;                  /* FITS file blocking factor */
   int deconv;                  /* Should clump parameters be deconvolved? */
   int dim[ NDF__MXDIM ];       /* Pixel axis dimensions */
   int dims[ 3 ];               /* Significant pixel axis dimensions */
   int el;                      /* Number of array elements mapped */
   int gotwcs;                  /* Does input NDF contain a WCS FrameSet? */
   int i;                       /* Loop count */
   int id;                      /* Clump identifier */
   int idmax;                   /* Maximum clump ID in supplied mask */
   int idmin;                   /* Minimum clump ID in supplied mask */
   int indf1;                   /* Identifier for input data NDF */
   int indf2;                   /* Identifier for input mask NDF */
   int indf3;                   /* Identifier for output mask NDF */
   int ishape;                  /* STC-S shape for spatial coverage */
   int jsacat;                  /* Is a JSA-style catalogue being created? */
   int ix;                      /* GRID value on 1st axis */
   int iy;                      /* GRID value on 2nd axis */
   int iz;                      /* GRID value on 3rd axis */
   int n;                       /* Number of values summed in "sum" */
   int nclump;                  /* Number of clump IDs */
   int nclumps;                 /* Number of stored clumps */
   int ndim;                    /* Total number of pixel axes */
   int nsig;                    /* Number of significant pixel axes */
   int nskyax;                  /* No. of sky axes in current WCS Frame */
   int nspecax;                 /* No. of spectral axes in current WCS Frame */
   int sdim[ NDF__MXDIM ];      /* The indices of the significant pixel axes */
   int skip[ 3 ];               /* Pixel axis skips */
   int slbnd[ NDF__MXDIM ];     /* The lower bounds of the significant pixel axes */
   int subnd[ NDF__MXDIM ];     /* The upper bounds of the significant pixel axes */
   int there;                   /* Does object exist? */
   int type;                    /* Integer identifier for data type */
   int usewcs;                  /* Use WCS coords in output catalogue? */
   int vax;                     /* Index of velocity WCS axis */
   int velax;                   /* Index of velocity pixel axis */
   size_t size;                 /* Number of elements in "grp" */
   void *ipd;                   /* Pointer to Data array */

/* Abort if an error has already occurred. */
   if( *status != SAI__OK ) return;

/* Initialise things to safe values. */
   xloc = NULL;
   ndfs = NULL;

/* Begin an AST context */
   astBegin;

/* Start an NDF context */
   ndfBegin();

/* Get an identifier for the two input NDFs. We use NDG (via kpg1_Rgndf)
   instead of calling ndfAssoc directly since NDF/HDS has problems with
   file names containing spaces, which NDG does not have. */
   kpg1Rgndf( "MASK", 1, 1, "", &grp, &size, status );
   ndgNdfas( grp, 1, "READ", &indf2, status );
   grpDelet( &grp, status );

   kpg1Rgndf( "DATA", 1, 1, "", &grp, &size, status );
   ndgNdfas( grp, 1, "READ", &indf1, status );
   grpDelet( &grp, status );

/* Get the Unit component. */
   dataunits[ 0 ] = 0;
   ndfCget( indf1, "Units", dataunits, 20, status );

/* Match the bounds of the two NDFs. */
   ndfMbnd( "TRIM", &indf1, &indf2, status );

/* Get the dimensions of the NDF, and count the significant ones. */
   ndfDim( indf1, NDF__MXDIM, dim, &ndim, status );
   nsig = 0;
   for( i = 0; i < ndim; i++ ) {
      if( dim[ i ] > 1 ) nsig++;
   }

/* Abort if the NDF is not 1-, 2- or 3- dimensional. */
   if( nsig > 3 ) {
      if( *status == SAI__OK ) {
         *status = SAI__ERROR;
         msgSeti( "N", nsig );
         errRep( "", "Supplied NDFs have ^N significant pixel axes", status );
         errRep( "", "This application requires 1, 2 or 3 significant pixel axes", status );
      }
   }

/* Get the WCS FrameSet and the significant axis bounds. */
   kpg1Asget( indf1, nsig, 1, 0, 0, sdim, slbnd, subnd, &iwcs, status );

/* Find the size of each dimension of the data array, and the skip in 1D
   vector index needed to move by pixel along an axis. */
   for( i = 0; i < nsig; i++ ) {
      dims[ i ] = subnd[ i ] - slbnd[ i ] + 1;
      skip[ i ] = ( i == 0 ) ? 1 : skip[ i - 1 ]*dims[ i - 1 ];
   }
   for( ; i < 3; i++ ) {
      dims[ i ] = 1;
      skip[ i ] = 0;
   }

/* Count the number of sky axes and spectral axes in the current Frame of
   the input NDFs WCS FrameSet. Also note the index of the spectral WCS
   axis. */
   nskyax = 0;
   nspecax = 0;
   vax = -1;
   for( i = 0; i < nsig; i++ ) {
      sprintf( attr, "Domain(%d)", i + 1 );
      dom = astGetC( iwcs, attr );
      if( dom ) {
         if( !strcmp( dom, "SKY" ) ) {
            nskyax++;
         } else if( !strcmp( dom, "SPECTRUM" ) ||
                    !strcmp( dom, "DSBSPECTRUM" ) ) {
            nspecax++;
            vax = i;
         }
      }
   }

/* Identify the pixel axis corresponding to the spectral WCS axis. Note,
   astMapSplit uses one-based axis indices, so we need to convert to and
   from zero-based for further use. */
   if( vax != -1 ) {
      map = astGetMapping( iwcs, AST__CURRENT, AST__BASE );
      vax++;
      astMapSplit( map, 1, &vax, &velax, &tmap );
      if( tmap ) {
         tmap = astAnnul( tmap );
         velax--;
      } else {
         velax = -1;
      }
      map = astAnnul( map );
   } else {
      velax = -1;
   }

/* See if a log file is to be created. */
   if( *status == SAI__OK ) {
      parGet0c( "LOGFILE", logfilename, GRP__SZNAM, status );
      if( *status == PAR__NULL ) {
         errAnnul( status );
      } else if( *status == SAI__OK ) {
         logfile = fopen( logfilename, "w" );
      }
   }

/* See if a JSA-style output catalogue is being created. */
   jsacat = 0;
   if( *status == SAI__OK ) {
      parGet0c( "JSACAT", buffer, sizeof( buffer ), status );
      if( *status == SAI__OK ) {
         jsacat = 1;
      } else if( *status == PAR__NULL ) {
         errAnnul( status );
      }
   }
/* If so, report an error unless the WCS of the input NDF contains a pair
   of sky axes. */
   if( jsacat && nskyax != 2 && *status == SAI__OK ) {
      *status = SAI__ERROR;
      errRepf( " ", "Cannot create a JSA-style output catalogue since "
               "the input NDF does not have any WCS sky axes.", status );
   }


/* See if the clump parameters are to be described using WCS values or
   pixel values. The default is yes if the current WCS Frame consists
   entirely of sky and spectral axes, and does not contain more than 1
   spectral axis and 2 sky axes. */
   parDef0l( "WCSPAR", ( nsig == 1 && nspecax == 1 && nskyax == 0 ) ||
                       ( nsig == 2 && nspecax == 0 && nskyax == 2 ) ||
                       ( nsig == 3 && nspecax == 1 && nskyax == 2 ),
             status );
   parGet0l( "WCSPAR", &usewcs, status );

/* See what STC-S shape should be used to describe each spatial clump. */
   ishape = 0;
   parChoic( "SHAPE", jsacat ? "Polygon" : "None", "Ellipse,Polygon,None", 1,
             shape, 10, status );
   if( *status == SAI__OK ) {
      if( !strcmp( shape, "POLYGON" ) ) {
         ishape = 2;
      } else if( !strcmp( shape, "ELLIPSE" ) ) {
         ishape = 1;
      }
   }

/* Report an error if we are creating a JSA-style catalogue and the user
   has selected not to use polygon shapes. */
   if( jsacat && ishape != 2 && *status == SAI__OK ) {
      *status = SAI__ERROR;
      errRepf( " ", "Cannot create a JSA-style output catalogue since "
               "the SHAPE parameter is not set to 'Polygon'.", status );
   }

/* Report an error if an attempt is made to produce STC-S descriptions of
   the spatial coverage of each clump using pixel coords. */
   if( ishape && *status == SAI__OK ) {
      if( nskyax < 2 ) {
         msgSetc( "S", shape );
         msgSetc( "S", "s" );
         *status = SAI__ERROR;
         errRep( " ", "Cannot produce STC-S ^S: the current WCS frame in "
                 "the input does not contain a pair of celestial sky axes.",
                 status );

      } else if( !usewcs ) {
         msgSetc( "S", shape );
         msgSetc( "S", "s" );
         *status = SAI__ERROR;
         errRep( " ", "Cannot produce STC-S ^S: the WCSPAR parameter "
                 "must be set TRUE to produce spatial regions.", status );
      }
   }

/* Choose the data type to use when mapping the DATA Data array. */
   ndfMtype( "_REAL,_DOUBLE", indf1, indf1, "DATA", itype, 20, dtype, 20,
             status );
   if( !strcmp( itype, "_DOUBLE" ) ) {
      type = CUPID__DOUBLE;
   } else {
      type = CUPID__FLOAT;
   }

/* Map the DATA Data array. */
   ndfMap( indf1, "DATA", itype, "READ", &ipd, &el, status );

/* Create the output NDF from the MASK NDF. */
   ndfProp( indf2, "AXIS,WCS", "OUT", &indf3, status );

/* Map the input mask array. */
   ndfMap( indf2, "DATA", "_INTEGER", "READ", (void *) &ipa, &el, status );

/* Find the largest and smallest clump identifier values in the output Data
   array. */
   if( *status == SAI__OK ) {
      idmax = VAL__MINI;
      idmin = VAL__MAXI;
      pid = ipa;
      for( i = 0; i < el; i++, pid++ ) {
         if( *pid != VAL__BADI ) {
            if( *pid < idmin ) idmin = *pid;
            if( *pid > idmax ) idmax = *pid;
         }
      }

      if( idmax < idmin ) {
         *status = SAI__ERROR;
         ndfMsg( "M", indf2 );
         errRep( "", "No clumps identified by mask NDF ^M", status );
      }

/* Store the number of clumps */
      nclump = idmax - idmin + 1;

   } else {
      nclump = 0;
      idmax = -1;
      idmin = 0;
   }

/* Allocate memory to hold the pixel bounds of the clumps. */
   clbnd = astMalloc( sizeof( int )*nclump*3 );
   cubnd = astMalloc( sizeof( int )*nclump*3 );

/* Find the upper and lower pixel bounds of each clump. */
   if( cubnd ) {

/* Initialise the bounding boxes. */
      for( i = 0; i < nclump*3; i++ ) {
         clbnd[ i ] = VAL__MAXI;
         cubnd[ i ] = VAL__MINI;
      }

/* Loop round every pixel in the pixel assignment array. */
      pa = ipa;
      for( iz = 1; iz <= dims[ 2 ]; iz++ ){
         for( iy = 1; iy <= dims[ 1 ]; iy++ ){
            for( ix = 1; ix <= dims[ 0 ]; ix++, pa++ ){

/* Skip pixels which are not in any clump. */
               if( *pa >= 0 ) {

/* Get the index within the clbnd and cubnd arrays of the current bounds
   on the x axis for this clump. */
                  i = 3*( *pa - idmin );

/* Update the bounds for the x axis, then increment to get the index of
   the y axis bounds. */
                  if( ix < clbnd[ i ] ) clbnd[ i ] = ix;
                  if( ix > cubnd[ i ] ) cubnd[ i ] = ix;
                  i++;

/* Update the bounds for the y axis, then increment to get the index of
   the z axis bounds. */
                  if( iy < clbnd[ i ] ) clbnd[ i ] = iy;
                  if( iy > cubnd[ i ] ) cubnd[ i ] = iy;
                  i++;

/* Update the bounds for the z axis. */
                  if( iz < clbnd[ i ] ) clbnd[ i ] = iz;
                  if( iz > cubnd[ i ] ) cubnd[ i ] = iz;
               }
            }
         }
      }

/* Loop round each clump, creating an NDF containing a description of the
   clump. */
      for( id = idmin; id <= idmax; id++ ) {
         i = 3*( id - idmin );
         ndfs = cupidNdfClump( type, ipd, ipa, el, nsig, dims, skip, slbnd, id,
                               clbnd + i, cubnd + i, NULL, ndfs,
                               VAL__MAXI, status );
      }
   }

/* Unmap the input pixel assignment array. */
   ndfUnmap( indf2, "*", status );

/* Skip the rest if no clumps were found. */
   if( ndfs ) {

/* Get a locator for the CUPID extension in the output NDF, creating a
   new one if none exists. Erase any CLUMPS component from the extension. */
      ndfXstat( indf3, "CUPID", &there, status );
      if( there ) {
         ndfXloc( indf3, "CUPID", "UPDATE", &xloc, status );
         datErase( xloc, "CLUMPS", status );
      } else {
         ndfXnew( indf3, "CUPID", "CUPID_EXT", 0, NULL, &xloc, status );
      }

/* Retrieve any configuration parameters from the CUPID extension. */
      config = cupidRetrieveConfig( xloc, status );

/* Get the beam sizes from the CONFIG array in the CUPID extension. */
      mconfig = NULL;
      method = "FELLWALKER";
      astMapGet0A( config, method, &mconfig );

      if( !mconfig ) {
         method = "CLUMPFIND";
         astMapGet0A( config, method, &mconfig );
      }

      if( !mconfig ) {
         method = "REINHOLD";
         astMapGet0A( config, method, &mconfig );
      }

      if( !mconfig ) {
         method = "GAUSSCLUMPS";
         astMapGet0A( config, method, &mconfig );
      }

      if( mconfig ) {
         if( !astMapGet0D( (AstKeyMap *) mconfig, "FWHMBEAM", &fb ) ) fb = 2.0;
         if( !astMapGet0D( (AstKeyMap *) mconfig, "VELORES", &vr ) ) vr = 2.0;
      } else {
         method = "";
         fb = 2.0;
         vr = 2.0;
      }

/* Allow the user to specify alternate values. */
      parDef0d( "FWHMBEAM", fb, status );
      parGet0d( "FWHMBEAM", &fb, status );
      beamcorr[ 0 ] = fb;
      beamcorr[ 1 ] = fb;

      if( ndim > 2 ) {
         parDef0d( "VELORES", vr, status );
         parGet0d( "VELORES", &vr, status );
         beamcorr[ 2 ] = vr;
      }

/* See if clump parameters should be deconvolved. */
      parGet0l( "DECONV", &deconv, status );

/* See if the background level is to be subtracted from the clump data
   values before calculating the clump sizes and centroid position. */
      parGet0l( "BACKOFF", &backoff, status );

/* Issue a logfile header for the clump parameters. */
      if( logfile ) {
         fprintf( logfile, "           Clump properties:\n" );
         fprintf( logfile, "           =================\n\n" );
      }

/* Store the clump properties in the CUPID extension and output catalogue
   (if needed). */
      ndfState( indf1, "WCS", &gotwcs, status );
      msgBlank( status );
      cupidStoreClumps( "OUTCAT", "JSACAT", indf1, xloc, ndfs, nsig, deconv,
                        backoff, ishape, velax, beamcorr, "Output from CUPID:EXTRACTCLUMPS",
                        usewcs, gotwcs ? iwcs : NULL, dataunits, NULL, logfile,
                        &nclumps, status );

/* Map the output pixel assignment array. */
      ndfMap( indf3, "DATA", "_INTEGER", "WRITE", (void *) &ipa, &el, status );
      ndfSbad( 1, indf3, "DATA", status );

/* Allocate room for a mask holding bad values for points which are not
   inside any clump. */
      rmask = astMalloc( sizeof( float )*(size_t) el );

/* Create the output data array by summing the contents of the NDFs describing
   the  found and usable clumps. This also fills the above mask array. */
      cupidSumClumps( type, ipd, nsig, slbnd, subnd, el, ndfs,
                      rmask, ipa, method, status );

/* Delete any existing quality name information from the output NDF, and
   create a structure to hold new quality name info. */
      irqDelet( indf3, status );
      irqNew( indf3, "CUPID", &qlocs, status );

/* Add in three quality names; "CLUMP", "BACKGROUND" and "EDGE". */
      irqAddqn( qlocs, "CLUMP", 0, "set iff a pixel is within a clump",
                status );
      irqAddqn( qlocs, "BACKGROUND", 0, "set iff a pixel is not within a clump",
                status );
      irqAddqn( qlocs, "EDGE", 0, "set iff a pixel is on the edge of a clump",
                status );

/* Transfer the pixel mask to the NDF quality array. */
      irqSetqm( qlocs, 1, "BACKGROUND", el, rmask, &n, status );
      irqSetqm( qlocs, 0, "CLUMP", el, rmask, &n, status );

/* Find the edges of the clumps (all other pixels will be set to
   VAL__BADR in "rmask"), and then set the "EDGE" Quality flag. */
      cupidEdges( rmask, el, dims, skip, 1.0, VAL__BADR, status );
      irqSetqm( qlocs, 0, "EDGE", el, rmask, &n, status );

/* Release the quality name information. */
      rmask = astFree( rmask );
      irqRlse( &qlocs, status );

/* Relase the extension locator.*/
      datAnnul( &xloc, status );
   }


/* Now we add history to any output JSA-style catalogue. We leave it
   until now to be sure the main output NDF is complete. We copy the
   HISTORY information from the main output NDF to the output JSA
   catalogue. */
   if( jsacat && nclumps ) {

/* Ensure default history has been written to the main output NDF. */
      ndfHdef( indf3, " ", status );

/* Re-open the output JSA catalogue. */
      cvgAssoc( "JSACAT", "Update", &fptr, &blockf, status );

/* Copy History from the main output NDF to the output catalogue. */
      cvgWhisr( indf3, fptr, status );

/* Add CHECKSUM and DATASUM headers. */
      if( *status == SAI__OK ) {
         int fstat = 0;
         ffpcks( fptr, &fstat );
      }

/* Close the FITS file. */
      cvgClose( &fptr, status );
   }

/* Tidy up */
L999:

/* Release the HDS object containing the list of NDFs describing the clumps. */
   if( ndfs ) datAnnul( &ndfs, status );

/* Free other resources. */
   clbnd = astFree( clbnd );
   cubnd = astFree( cubnd );

/* End the NDF context */
   ndfEnd( status );

/* Close any log file. */
   if( logfile ) fclose( logfile );

/* End the AST context */
   astEnd;

/* If an error has occurred, issue another error report identifying the
   program which has failed (i.e. this one). */
   if( *status != SAI__OK ) {
      errRep( "EXTRACTCLUMPS_ERR", "EXTRACTCLUMPS: Failed to extract clumps "
              "of emission from a 1, 2 or 3-D NDF.", status );
   }

}
