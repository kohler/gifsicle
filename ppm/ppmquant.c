/* ppmquant.c - quantize the colors in a pixmap down to a specified number
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "ppm.h"
#include "ppmcmap.h"

#define MAXCOLORS 32767

/* #define LARGE_NORM */
#define LARGE_LUM

/* #define REP_CENTER_BOX */
/* #define REP_AVERAGE_COLORS */
#define REP_AVERAGE_PIXELS

typedef struct box* box_vector;
struct box
    {
    int ind;
    int colors;
    int sum;
    };

static colorhist_vector mediancut ARGS(( colorhist_vector chv, int colors, int sum, pixval maxval, int newcolors ));
static int redcompare ARGS(( colorhist_vector ch1, colorhist_vector ch2 ));
static int greencompare ARGS(( colorhist_vector ch1, colorhist_vector ch2 ));
static int bluecompare ARGS(( colorhist_vector ch1, colorhist_vector ch2 ));
static int sumcompare ARGS(( box_vector b1, box_vector b2 ));

void
main( argc, argv )
    int argc;
    char* argv[];
    {
    FILE* ifp;
    pixel** pixels;
    pixel** mappixels;
    register pixel* pP;
    int argn, rows, cols, maprows, mapcols, row;
    register int col, limitcol;
    pixval maxval, newmaxval, mapmaxval;
    int newcolors, colors;
    register int ind;
    colorhist_vector chv, colormap;
    colorhash_table cht;
    int floyd;
    int usehash;
    long* thisrerr;
    long* nextrerr;
    long* thisgerr;
    long* nextgerr;
    long* thisberr;
    long* nextberr;
    long* temperr;
    register long sr, sg, sb, err;
#define FS_SCALE 1024
    int fs_direction;
    char* usage = "[-floyd|-fs] <ncolors> [ppmfile]\n                 [-floyd|-fs] -map mapfile [ppmfile]";

    ppm_init( &argc, argv );

    argn = 1;
    floyd = 0;
    mappixels = (pixel**) 0;

    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' )
	{
	if ( pm_keymatch( argv[argn], "-fs", 2 ) ||
	     pm_keymatch( argv[argn], "-floyd", 2 ) )
	    floyd = 1;
	else if ( pm_keymatch( argv[argn], "-nofs", 2 ) ||
	     pm_keymatch( argv[argn], "-nofloyd", 2 ) )
	    floyd = 0;
	else if ( pm_keymatch( argv[argn], "-map", 2 ) )
	    {
	    ++argn;
	    if ( argn == argc )
		pm_usage( usage );
	    ifp = pm_openr( argv[argn] );
	    mappixels = ppm_readppm( ifp, &mapcols, &maprows, &mapmaxval );
	    pm_close( ifp );
	    if ( mapcols == 0 || maprows == 0 )
		pm_error( "null colormap??" );
	    }
	else
	    pm_usage( usage );
	++argn;
	}

    if ( mappixels == (pixel**) 0 )
	{
	if ( argn == argc )
	    pm_usage( usage );
	if ( sscanf( argv[argn], "%d", &newcolors ) != 1 )
	    pm_usage( usage );
	if ( newcolors <= 1 )
	    pm_error( "number of colors must be > 1" );
	++argn;
	}

    if ( argn != argc )
	{
	ifp = pm_openr( argv[argn] );
	++argn;
	}
    else
	ifp = stdin;

    if ( argn != argc )
	pm_usage( usage );

    /*
    ** Step 1: read in the image.
    */
    pixels = ppm_readppm( ifp, &cols, &rows, &maxval );
    pm_close( ifp );


    if ( mappixels == (pixel**) 0 )
	{
	/*
	** Step 2: attempt to make a histogram of the colors, unclustered.
	** If at first we don't succeed, lower maxval to increase color
	** coherence and try again.  This will eventually terminate, with
	** maxval at worst 15, since 32^3 is approximately MAXCOLORS.
	*/
	for ( ; ; )
	    {
	    pm_message( "making histogram..." );
	    chv = ppm_computecolorhist(
		pixels, cols, rows, MAXCOLORS, &colors );
	    if ( chv != (colorhist_vector) 0 )
		break;
	    pm_message( "too many colors!" );
	    newmaxval = maxval / 2;
	    pm_message(
	  "scaling colors from maxval=%d to maxval=%d to improve clustering...",
		    maxval, newmaxval );
	    for ( row = 0; row < rows; ++row )
		for ( col = 0, pP = pixels[row]; col < cols; ++col, ++pP )
		    PPM_DEPTH( *pP, *pP, maxval, newmaxval );
	    maxval = newmaxval;
	    }
	pm_message( "%d colors found", colors );

	/*
	** Step 3: apply median-cut to histogram, making the new colormap.
	*/
	pm_message( "choosing %d colors...", newcolors );
	colormap = mediancut( chv, colors, rows * cols, maxval, newcolors );
	ppm_freecolorhist( chv );
	}
    else
	{
	/*
	** Alternate steps 2 & 3 : Turn mappixels into a colormap.
	*/
	if ( mapmaxval != maxval )
	    {
	    if ( mapmaxval > maxval )
		pm_message( "rescaling colormap colors" );
	    for ( row = 0; row < maprows; ++row )
		for ( col = 0, pP = mappixels[row]; col < mapcols; ++col, ++pP )
		    PPM_DEPTH( *pP, *pP, mapmaxval, maxval );
	    mapmaxval = maxval;
	    }
	colormap = ppm_computecolorhist(
	    mappixels, mapcols, maprows, MAXCOLORS, &newcolors );
	if ( colormap == (colorhist_vector) 0 )
	    pm_error( "too many colors in colormap!" );
	ppm_freearray( mappixels, maprows );
	pm_message( "%d colors found in colormap", newcolors );
	}

    /*
    ** Step 4: map the colors in the image to their closest match in the
    ** new colormap, and write 'em out.
    */
    pm_message( "mapping image to new colors..." );
    cht = ppm_alloccolorhash( );
    usehash = 1;
    ppm_writeppminit( stdout, cols, rows, maxval, 0 );
    if ( floyd )
	{
	/* Initialize Floyd-Steinberg error vectors. */
	thisrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	thisgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	thisberr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextberr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	srandom( (int) ( time( 0 ) ^ getpid( ) ) );
	for ( col = 0; col < cols + 2; ++col )
	    {
	    thisrerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    thisgerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    thisberr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    /* (random errors in [-1 .. 1]) */
	    }
	fs_direction = 1;
	}
    for ( row = 0; row < rows; ++row )
	{
	if ( floyd )
	    for ( col = 0; col < cols + 2; ++col )
		nextrerr[col] = nextgerr[col] = nextberr[col] = 0;
	if ( ( ! floyd ) || fs_direction )
	    {
	    col = 0;
	    limitcol = cols;
	    pP = pixels[row];
	    }
	else
	    {
	    col = cols - 1;
	    limitcol = -1;
	    pP = &(pixels[row][col]);
	    }
	do
	    {
	    if ( floyd )
		{
		/* Use Floyd-Steinberg errors to adjust actual color. */
		sr = PPM_GETR(*pP) + thisrerr[col + 1] / FS_SCALE;
		sg = PPM_GETG(*pP) + thisgerr[col + 1] / FS_SCALE;
		sb = PPM_GETB(*pP) + thisberr[col + 1] / FS_SCALE;
		if ( sr < 0 ) sr = 0;
		else if ( sr > maxval ) sr = maxval;
		if ( sg < 0 ) sg = 0;
		else if ( sg > maxval ) sg = maxval;
		if ( sb < 0 ) sb = 0;
		else if ( sb > maxval ) sb = maxval;
		PPM_ASSIGN( *pP, sr, sg, sb );
		}

	    /* Check hash table to see if we have already matched this color. */
	    ind = ppm_lookupcolor( cht, pP );
	    if ( ind == -1 )
		{ /* No; search colormap for closest match. */
		register int i, r1, g1, b1, r2, g2, b2;
		register long dist, newdist;
		r1 = PPM_GETR( *pP );
		g1 = PPM_GETG( *pP );
		b1 = PPM_GETB( *pP );
		dist = 2000000000;
		for ( i = 0; i < newcolors; ++i )
		    {
		    r2 = PPM_GETR( colormap[i].color );
		    g2 = PPM_GETG( colormap[i].color );
		    b2 = PPM_GETB( colormap[i].color );
		    newdist = ( r1 - r2 ) * ( r1 - r2 ) +
			      ( g1 - g2 ) * ( g1 - g2 ) +
			      ( b1 - b2 ) * ( b1 - b2 );
		    if ( newdist < dist )
			{
			ind = i;
			dist = newdist;
			}
		    }
		if ( usehash )
		    {
		    if ( ppm_addtocolorhash( cht, pP, ind ) < 0 )
			{
			pm_message(
		   "out of memory adding to hash table, proceeding without it");
			usehash = 0;
			}
		    }
		}

	    if ( floyd )
		{
		/* Propagate Floyd-Steinberg error terms. */
		if ( fs_direction )
		    {
		    err = ( sr - (long) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
		    thisrerr[col + 2] += ( err * 7 ) / 16;
		    nextrerr[col    ] += ( err * 3 ) / 16;
		    nextrerr[col + 1] += ( err * 5 ) / 16;
		    nextrerr[col + 2] += ( err     ) / 16;
		    err = ( sg - (long) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
		    thisgerr[col + 2] += ( err * 7 ) / 16;
		    nextgerr[col    ] += ( err * 3 ) / 16;
		    nextgerr[col + 1] += ( err * 5 ) / 16;
		    nextgerr[col + 2] += ( err     ) / 16;
		    err = ( sb - (long) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
		    thisberr[col + 2] += ( err * 7 ) / 16;
		    nextberr[col    ] += ( err * 3 ) / 16;
		    nextberr[col + 1] += ( err * 5 ) / 16;
		    nextberr[col + 2] += ( err     ) / 16;
		    }
		else
		    {
		    err = ( sr - (long) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
		    thisrerr[col    ] += ( err * 7 ) / 16;
		    nextrerr[col + 2] += ( err * 3 ) / 16;
		    nextrerr[col + 1] += ( err * 5 ) / 16;
		    nextrerr[col    ] += ( err     ) / 16;
		    err = ( sg - (long) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
		    thisgerr[col    ] += ( err * 7 ) / 16;
		    nextgerr[col + 2] += ( err * 3 ) / 16;
		    nextgerr[col + 1] += ( err * 5 ) / 16;
		    nextgerr[col    ] += ( err     ) / 16;
		    err = ( sb - (long) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
		    thisberr[col    ] += ( err * 7 ) / 16;
		    nextberr[col + 2] += ( err * 3 ) / 16;
		    nextberr[col + 1] += ( err * 5 ) / 16;
		    nextberr[col    ] += ( err     ) / 16;
		    }
		}

	    *pP = colormap[ind].color;

	    if ( ( ! floyd ) || fs_direction )
		{
		++col;
		++pP;
		}
	    else
		{
		--col;
		--pP;
		}
	    }
	while ( col != limitcol );

	if ( floyd )
	    {
	    temperr = thisrerr;
	    thisrerr = nextrerr;
	    nextrerr = temperr;
	    temperr = thisgerr;
	    thisgerr = nextgerr;
	    nextgerr = temperr;
	    temperr = thisberr;
	    thisberr = nextberr;
	    nextberr = temperr;
	    fs_direction = ! fs_direction;
	    }

	ppm_writeppmrow( stdout, pixels[row], cols, maxval, 0 );
	}

    pm_close( stdout );

    exit( 0 );
    }

/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

#if __STDC__
static colorhist_vector
mediancut( colorhist_vector chv, int colors, int sum, pixval maxval, int newcolors )
#else /*__STDC__*/
static colorhist_vector
mediancut( chv, colors, sum, maxval, newcolors )
    colorhist_vector chv;
    int colors, sum, newcolors;
    pixval maxval;
#endif /*__STDC__*/
    {
    colorhist_vector colormap;
    box_vector bv;
    register int bi, i;
    int boxes;

    for (i = 0; i < colors; i++)
      fprintf(stderr, "#%02X%02X%02X %d  ", PPM_GETR(chv[i].color),
	      PPM_GETG(chv[i].color),PPM_GETB(chv[i].color),
	      chv[i].value);
    fprintf(stderr, "\n");
    bv = (box_vector) malloc( sizeof(struct box) * newcolors );
    colormap =
	(colorhist_vector) malloc( sizeof(struct colorhist_item) * newcolors );
    if ( bv == (box_vector) 0 || colormap == (colorhist_vector) 0 )
	pm_error( "out of memory" );
    for ( i = 0; i < newcolors; ++i )
	PPM_ASSIGN( colormap[i].color, 0, 0, 0 );

    /*
    ** Set up the initial box.
    */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;
    boxes = 1;

    /*
    ** Main loop: split boxes until we have enough.
    */
    while ( boxes < newcolors )
	{
	register int indx, clrs;
	int sm;
	register int minr, maxr, ming, maxg, minb, maxb, v;
	int halfsum, lowersum;

	/*
	** Find the first splittable box.
	*/
	for ( bi = 0; bi < boxes; ++bi )
	    if ( bv[bi].colors >= 2 )
		break;
	if ( bi == boxes )
	    break;	/* ran out of colors! */
	indx = bv[bi].ind;
	clrs = bv[bi].colors;
	sm = bv[bi].sum;
    fprintf(stderr, "splitting %d / %d+%u : ", bi,
	    clrs, sm);

	/*
	** Go through the box finding the minimum and maximum of each
	** component - the boundaries of the box.
	*/
	minr = maxr = PPM_GETR( chv[indx].color );
	ming = maxg = PPM_GETG( chv[indx].color );
	minb = maxb = PPM_GETB( chv[indx].color );
	for ( i = 1; i < clrs; ++i )
	    {
	    v = PPM_GETR( chv[indx + i].color );
	    if ( v < minr ) minr = v;
	    if ( v > maxr ) maxr = v;
	    v = PPM_GETG( chv[indx + i].color );
	    if ( v < ming ) ming = v;
	    if ( v > maxg ) maxg = v;
	    v = PPM_GETB( chv[indx + i].color );
	    if ( v < minb ) minb = v;
	    if ( v > maxb ) maxb = v;
	    }
    fprintf(stderr, "#%02X%02X%02X - %02X%02X%02X\n", minr, ming,
	    minb, maxr, maxg, maxb);

	/*
	** Find the largest dimension, and sort by that component.  I have
	** included two methods for determining the "largest" dimension;
	** first by simply comparing the range in RGB space, and second
	** by transforming into luminosities before the comparison.  You
	** can switch which method is used by switching the commenting on
	** the LARGE_ defines at the beginning of this source file.
	*/
#ifdef LARGE_NORM
	if ( maxr - minr >= maxg - ming && maxr - minr >= maxb - minb )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		redcompare );
	else if ( maxg - ming >= maxb - minb )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		greencompare );
	else
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		bluecompare );
#endif /*LARGE_NORM*/
#ifdef LARGE_LUM
	{
	pixel p;
	float rl, gl, bl;

	PPM_ASSIGN(p, maxr - minr, 0, 0);
	rl = PPM_LUMIN(p);
	PPM_ASSIGN(p, 0, maxg - ming, 0);
	gl = PPM_LUMIN(p);
	PPM_ASSIGN(p, 0, 0, maxb - minb);
	bl = PPM_LUMIN(p);

	if ( rl >= gl && rl >= bl )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		redcompare );
	else if ( gl >= bl )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		greencompare );
	else
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
		bluecompare );
	}
#endif /*LARGE_LUM*/
	
	/*
	** Now find the median based on the counts, so that about half the
	** pixels (not colors, pixels) are in each subdivision.
	*/
	lowersum = chv[indx].value;
	halfsum = sm / 2;
	for ( i = 1; i < clrs - 1; ++i )
	    {
	    if ( lowersum >= halfsum )
		break;
	    lowersum += chv[indx + i].value;
    fprintf(stderr, "#%02X%02X%02X/%d ", PPM_GETR(chv[indx+i].color),
	    PPM_GETG(chv[indx+i].color),PPM_GETB(chv[indx+i].color),
	    chv[indx+i].value);
	    }
	fprintf(stderr,"\n");

	/*
	** Split the box, and sort to bring the biggest boxes to the top.
	*/
	bv[bi].colors = i;
	bv[bi].sum = lowersum;
	bv[boxes].ind = indx + i;
	bv[boxes].colors = clrs - i;
	bv[boxes].sum = sm - lowersum;
      fprintf(stderr, "    -> %u *** %d / %u\n", bv[bi].sum,
	      boxes, bv[boxes].sum);
	++boxes;
	qsort( (char*) bv, boxes, sizeof(struct box), sumcompare );
	}

    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.  You can switch which
    ** method is used by switching the commenting on the REP_ defines at
    ** the beginning of this source file.
    */
    for ( bi = 0; bi < boxes; ++bi )
	{
#ifdef REP_CENTER_BOX
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register int minr, maxr, ming, maxg, minb, maxb, v;

	minr = maxr = PPM_GETR( chv[indx].color );
	ming = maxg = PPM_GETG( chv[indx].color );
	minb = maxb = PPM_GETB( chv[indx].color );
	for ( i = 1; i < clrs; ++i )
	    {
	    v = PPM_GETR( chv[indx + i].color );
	    minr = min( minr, v );
	    maxr = max( maxr, v );
	    v = PPM_GETG( chv[indx + i].color );
	    ming = min( ming, v );
	    maxg = max( maxg, v );
	    v = PPM_GETB( chv[indx + i].color );
	    minb = min( minb, v );
	    maxb = max( maxb, v );
	    }
	PPM_ASSIGN(
	    colormap[bi].color, ( minr + maxr ) / 2, ( ming + maxg ) / 2,
	    ( minb + maxb ) / 2 );
#endif /*REP_CENTER_BOX*/
#ifdef REP_AVERAGE_COLORS
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register long r = 0, g = 0, b = 0;

	for ( i = 0; i < clrs; ++i )
	    {
	    r += PPM_GETR( chv[indx + i].color );
	    g += PPM_GETG( chv[indx + i].color );
	    b += PPM_GETB( chv[indx + i].color );
	    }
	r = r / clrs;
	g = g / clrs;
	b = b / clrs;
	PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_COLORS*/
#ifdef REP_AVERAGE_PIXELS
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register long r = 0, g = 0, b = 0, sum = 0;

	for ( i = 0; i < clrs; ++i )
	    {
	    r += PPM_GETR( chv[indx + i].color ) * chv[indx + i].value;
	    g += PPM_GETG( chv[indx + i].color ) * chv[indx + i].value;
	    b += PPM_GETB( chv[indx + i].color ) * chv[indx + i].value;
	    sum += chv[indx + i].value;
	    }
	r = r / sum;
	if ( r > maxval ) r = maxval;	/* avoid math errors */
	g = g / sum;
	if ( g > maxval ) g = maxval;
	b = b / sum;
	if ( b > maxval ) b = maxval;
	PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_PIXELS*/
	}

    /*
    ** All done.
    */
    return colormap;
    }

static int
redcompare( ch1, ch2 )
    colorhist_vector ch1, ch2;
    {
    return (int) PPM_GETR( ch1->color ) - (int) PPM_GETR( ch2->color );
    }

static int
greencompare( ch1, ch2 )
    colorhist_vector ch1, ch2;
    {
    return (int) PPM_GETG( ch1->color ) - (int) PPM_GETG( ch2->color );
    }

static int
bluecompare( ch1, ch2 )
    colorhist_vector ch1, ch2;
    {
    return (int) PPM_GETB( ch1->color ) - (int) PPM_GETB( ch2->color );
    }

static int
sumcompare( b1, b2 )
    box_vector b1, b2;
    {
    return b2->sum - b1->sum;
    }
