/*
 * svg_stats.c: Funtions used by sadf to display statistics in SVG format.
 * (C) 2016 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <float.h>

#include "sa.h"
#include "sadf.h"
#include "ioconf.h"
#include "svg_stats.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

extern unsigned int flags;
extern unsigned int dm_major;

unsigned int svg_colors[] = {0x00cc00, 0xff00bf, 0x00ffff, 0xff0000,
			     0xe85f00, 0x0000ff, 0x006020, 0x7030a0,
			     0xffff00, 0x666635, 0xd60093, 0x00bfbf,
			     0xcc3300, 0xbfbfbf, 0xffffbf, 0xff3300};
#define SVG_COLORS_IDX_MASK	0x0f

/*
 ***************************************************************************
 * Compare the values of a statistics sample with the max and min values
 * already found in previous samples for this same activity. If some new
 * min or max values are found, then save them.
 * The structure containing the statistics sample is composed of @llu_nr
 * unsigned long long fields, followed by @lu_nr unsigned long fields, then
 * followed by @u_nr unsigned int fields.
 *
 * IN:
 * @llu_nr	Number of unsigned long long fields composing the structure.
 * @lu_nr	Number of unsigned long fields composing the structure.
 * @u_nr	Number of unsigned int fields composing the structure.
 * @cs		Pointer on current sample statistics structure.
 * @ps		Pointer on previous sample statistics structure (may be NULL).
 * @itv		Interval of time in jiffies.
 * @minv	Array containing min values already found for this activity.
 * @maxv	Array containing max values already found for this activity.
 *
 * OUT:
 * @minv	Array containg the possible new min values for current activity.
 * @maxv	Array containg the possible new max values for current activity.
 *
 * NB: @minv and @maxv arrays contain values in the same order as the fields
 * in the statistics structure.
 ***************************************************************************
 */
void save_extrema(int llu_nr, int lu_nr, int u_nr, void *cs, void *ps,
		  unsigned long long itv, double minv[], double maxv[])
{
	unsigned long long *lluc, *llup;
	unsigned long *luc, *lup;
	unsigned int *uc, *up;
	double val;
	int i, m = 0;

	/* Compare unsigned long long fields */
	lluc = (unsigned long long *) cs;
	llup = (unsigned long long *) ps;
	for (i = 0; i < llu_nr; i++, m++) {
		if (ps) {
			val = S_VALUE(*llup, *lluc, itv);
		}
		else {
			/*
			 * If no pointer on previous sample has been given
			 * then the value is not a per-second one.
			 */
			val = (double) *lluc;
		}
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		lluc = (unsigned long long *) ((char *) lluc + ULL_ALIGNMENT_WIDTH);
		if (ps) {
			llup = (unsigned long long *) ((char *) llup + ULL_ALIGNMENT_WIDTH);
		}
	}

	/* Compare unsigned long fields */
	luc = (unsigned long *) lluc;
	lup = (unsigned long *) llup;
	for (i = 0; i < lu_nr; i++, m++) {
		if (ps) {
			val = S_VALUE(*lup, *luc, itv);
		}
		else {
			val = (double) *luc;
		}
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		luc = (unsigned long *) ((char *) luc + UL_ALIGNMENT_WIDTH);
		if (ps) {
			lup = (unsigned long *) ((char *) lup + UL_ALIGNMENT_WIDTH);
		}
	}

	/* Compare unsigned int fields */
	uc = (unsigned int *) luc;
	up = (unsigned int *) lup;
	for (i = 0; i < u_nr; i++, m++) {
		if (ps) {
			val = S_VALUE(*up, *uc, itv);
		}
		else {
			val = (double) *uc;
		}
		if (val < minv[m]) {
			minv[m] = val;
		}
		if (val > maxv[m]) {
			maxv[m] = val;
		}
		uc = (unsigned int *) ((char *) uc + U_ALIGNMENT_WIDTH);
		if (ps) {
			up = (unsigned int *) ((char *) up + U_ALIGNMENT_WIDTH);
		}
	}
}

/*
 ***************************************************************************
 * Find the min and max values of all the graphs that will be drawn in the
 * same view. The graphs have their own min and max values in
 * minv[pos...pos+n-1] and maxv[pos...pos+n-1]. 
 *
 * IN:
 * @pos		Position in array for the first graph extrema value.
 * @n		Number of graphs to scan.
 * @minv	Array containing min values for graphs.
 * @maxv	Array containing max values for graphs.
 *
 * OUT:
 * @gmin	Global min value found.
 * @gmax	Global max value found.
 ***************************************************************************
 */
void get_global_extrema(int pos, int n, double minv[], double maxv[], double *gmin, double *gmax)
{
	int i;

	*gmin = minv[pos];
	*gmax = maxv[pos];

	for (i = 1; i < n; i++) {
		if (minv[pos + i] < *gmin) {
			*gmin = minv[pos + i];
		}
		if (maxv[pos + i] > *gmax) {
			*gmax = maxv[pos + i];
		}
	}
}

/*
 ***************************************************************************
 * Allocate arrays used to save graphs data, min and max values.
 * @n arrays of chars are allocated for @n graphs to draw. A pointer on this
 * array is returned. This is equivalent to "char data[][n]" where each
 * element is of indeterminate size and will contain the graph data (eg.
 * << path d="M12,14 L13,16..." ... >>.
 * The size of element data[i] is given by outsize[i].
 * Also allocate an array to save min values (equivalent to "double spmin[n]")
 * and an array for max values (equivalent to "double spmax[n]").
 *
 * IN:
 * @n		Number of graphs to draw for current activity.
 *
 * OUT:
 * @outsize	Array that will contain the sizes of each element in array
 *		of chars. Equivalent to "int outsize[n]" with
 * 		outsize[n] = sizeof(data[][n]).
 * @spmin	Array that will contain min values for current activity.
 * @spmax	Array that will contain max values for current activity.
 *
 * RETURNS:
 * Pointer on array of arrays of chars that will contain the graphs data.
 *
 * NB: @min and @max arrays contain values in the same order as the fields
 * in the statistics structure.
 ***************************************************************************
 */
char **allocate_graph_lines(int n, int **outsize, double **spmin, double **spmax)
{
	char **out;
	char *out_p;
	int i;

	/*
	 * Allocate an array of pointers. Each of these pointers will
	 * be an array of chars.
	 */
	if ((out = (char **) malloc(n * sizeof(char *))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the size of each array of chars */
	if ((*outsize = (int *) malloc(n * sizeof(int))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the min value of each graph */
	if ((*spmin = (double *) malloc(n * sizeof(double))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate array that will contain the max value of each graph */
	if ((*spmax = (double *) malloc(n * sizeof(double))) == NULL) {
		perror("malloc");
		exit(4);
	}
	/* Allocate arrays of chars that will contain graphs data */
	for (i = 0; i < n; i++) {
		if ((out_p = (char *) malloc(CHUNKSIZE * sizeof(char))) == NULL) {
			perror("malloc");
			exit(4);
		}
		*(out + i) = out_p;
		*out_p = '\0';			/* Reset string so that it can be safely strncat()'d later */
		*(*outsize + i) = CHUNKSIZE;	/* Each array of chars has a default size of CHUNKSIZE */
		*(*spmin + i) = DBL_MAX;	/* Init min and max values */
		*(*spmax + i) = -DBL_MAX;
	}

	return out;
}

/*
 ***************************************************************************
 * Save SVG code for current graph.
 *
 * IN:
 * @data	SVG code to append to current graph definition.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void save_svg_data(char *data, char **out, int *outsize)
{
	char *out_p;
	int len;

	out_p = *out;
	/* Determine space left in array */
	len = *outsize - strlen(out_p) - 1;
	if (strlen(data) >= len) {
		/*
		 * If current array of chars doesn't have enough space left
		 * then reallocate it with CHUNKSIZE more bytes.
		 */
		SREALLOC(out_p, char, *outsize + CHUNKSIZE);
		*out = out_p;
		*outsize += CHUNKSIZE;
		len += CHUNKSIZE;
	}
	strncat(out_p, data, len);
}

/*
 ***************************************************************************
 * Update line graph definition by appending current X,Y coordinates.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @value	Value of current sample metric. Will be used as Y coordinate.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @restart	Set to TRUE if a RESTART record has been read since the last
 * 		statistics sample.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void lnappend(unsigned long timetag, double value, char **out, int *outsize, int restart)
{
	char data[128];

	/* Prepare additional graph definition data */
	snprintf(data, 128, " %c%lu,%.2f", restart ? 'M' : 'L', timetag, value);
	data[127] = '\0';

	save_svg_data(data, out, outsize);
}

/*
 ***************************************************************************
 * Update line graph definition by appending current X,Y coordinates. Use
 * (unsigned long) integer values here.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @value	Value of current sample metric. Will be used as Y coordinate.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @restart	Set to TRUE if a RESTART record has been read since the last
 * 		statistics sample.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void lniappend(unsigned long timetag, unsigned long value, char **out, int *outsize,
	       int restart)
{
	char data[128];

	/* Prepare additional graph definition data */
	snprintf(data, 128, " %c%lu,%lu", restart ? 'M' : 'L', timetag, value);
	data[127] = '\0';

	save_svg_data(data, out, outsize);
}

/*
 ***************************************************************************
 * Update bar graph definition by adding a new rectangle.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @value	Value of current sample metric. Will be used as rectangle
 *		height.
 * @offset	Offset for Y coordinate.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @dt		Interval of time in seconds between current and previous
 * 		sample.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void brappend(unsigned long timetag, double offset, double value, char **out, int *outsize,
	      unsigned long dt)
{
	char data[128];

	/* Prepare additional graph definition data */
	if (value == 0.0)
		/* Dont draw a flat rectangle! */
		return;

	snprintf(data, 128, "<rect x=\"%lu\" y=\"%.2f\" height=\"%.2f\" width=\"%lu\"/>",
		 timetag - dt, MINIMUM(offset, 100.0), MINIMUM(value, (100.0 - offset)), dt);
	data[127] = '\0';

	save_svg_data(data, out, outsize);

}

/*
 ***************************************************************************
 * Update CPU graph and min/max values for each metric.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @offset	Offset for Y coordinate.
 * @value	Value of current CPU metric. Will be used as rectangle
 *		height.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @dt		Interval of time in seconds between current and previous
 * 		sample.
 * @spmin	Min value already found for this CPU metric.
 * @spmax	Max value already found for this CPU metric.
 *
 * OUT:
 * @offset	New offset value, to use to draw next rectangle
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 ***************************************************************************
 */
void cpuappend(unsigned long timetag, double *offset, double value, char **out, int *outsize,
	       unsigned long dt, double *spmin, double *spmax)
{
	/* Save min and max values */
	if (value < *spmin) {
		*spmin = value;
	}
	if (value > *spmax) {
		*spmax = value;
	}
	/* Prepare additional graph definition data */
	brappend(timetag, *offset, value, out, outsize, dt);

	*offset += value;
}

/*
 ***************************************************************************
 * Update rectangular graph and min/max values.
 *
 * IN:
 * @timetag	Timestamp in seconds since the epoch for current sample
 *		stats. Will be used as X coordinate.
 * @p_value	Metric value for previous sample
 * @value	Metric value for current sample.
 * @out		Pointer on array of chars for current graph definition.
 * @outsize	Size of array of chars for current graph definition.
 * @restart	Set to TRUE if a RESTART record has been read since the last
 * 		statistics sample.
 * @dt		Interval of time in seconds between current and previous
 * 		sample.
 * @spmin	Min value already found for this metric.
 * @spmax	Max value already found for this metric.
 *
 * OUT:
 * @out		Pointer on array of chars for current graph definition that
 *		has been updated with the addition of current sample data.
 * @outsize	Array that containing the (possibly new) sizes of each
 *		element in array of chars.
 * @spmin	Min value for this metric.
 * @spmax	Max value for this metric.
 ***************************************************************************
 */
void recappend(unsigned long timetag, double p_value, double value, char **out, int *outsize,
	       int restart, unsigned long dt, double *spmin, double *spmax)
{
	char data[128], data1[128], data2[128];

	/* Save min and max values */
	if (value < *spmin) {
		*spmin = value;
	}
	if (value > *spmax) {
		*spmax = value;
	}
	/* Prepare additional graph definition data */
	if (restart) {
		snprintf(data1, 128, " M%lu,%.2f", timetag - dt, p_value);
		data1[127] = '\0';
	}
	if (p_value != value) {
		snprintf(data2, 128, " L%lu,%.2f", timetag, value);
		data2[127] = '\0';
	}
	snprintf(data, 128, "%s L%lu,%.2f%s", restart ? data1 : "", timetag, p_value,
		 p_value != value ? data2 : "");
	data[127] = '\0';

	save_svg_data(data, out, outsize);
}

/*
 ***************************************************************************
 * Calculate 10 raised to the power of n.
 *
 * IN:
 * @n	Power number to use.
 *
 * RETURNS:
 * 10 raised to the power of n.
 ***************************************************************************
 */
unsigned int pwr10(int n)
{
	int i;
	unsigned int e = 1;

	for (i = 0; i < n; i++) {
		e = e * 10;
	}

	return e;
}

/*
 ***************************************************************************
 * Calculate the value on the Y axis between two horizontal lines that will
 * make the graph background grid.
 *
 * IN:
 * @lmax	Max value reached for this graph.
 *
 * OUT:
 * @dp		Number of decimal places for Y graduations.
 *
 * RETURNS:
 * Value between two horizontal lines.
 ***************************************************************************
 */
double ygrid(double lmax, int *dp)
{
	char val[32];
	int l;
	unsigned int e;
	long n = 0;

	*dp = 0;
	if (lmax == 0) {
		lmax = 1;
	}
	n = (long) (lmax / SVG_H_GRIDNR);
	if (!n) {
		*dp = 2;
		return (lmax / SVG_H_GRIDNR);
	}
	snprintf(val, 32, "%ld", n);
	val[31] = '\0';
	l = strlen(val);
	if (l < 2)
		return n;
	e = pwr10(l - 1);

	return ((double) (((long) (n / e)) * e));
}

/*
 ***************************************************************************
 * Calculate the value on the X axis between two vertical lines that will
 * make the graph background grid.
 *
 * IN:
 * @timestart	First data timestamp (X coordinate of the first data point).
 * @timeend	Last data timestamp (X coordinate of the last data point).
 * @v_gridnr	Number of vertical lines to display. Its value is normally
 *		SVG_V_GRIDNR, except when option "oneday" is used, in which
 *		case it is set to 12.
 *
 * RETURNS:
 * Value between two vertical lines.
 ***************************************************************************
 */
long int xgrid(unsigned long timestart, unsigned long timeend, int v_gridnr)
{
	if ((timeend - timestart) <= v_gridnr)
		return 1;
	else
		return ((timeend - timestart) / v_gridnr);
}

/*
 ***************************************************************************
 * Free global graphs structures.
 *
 * IN:
 * @out		Pointer on array of chars for each graph definition.
 * @outsize	Size of array of chars for each graph definition.
 * @spmin	Array containing min values for graphs.
 * @spmax	Array containing max values for graphs.
 ***************************************************************************
 */
void free_graphs(char **out, int *outsize, double *spmin, double *spmax)
{
	if (out) {
		free(out);
	}
	if (outsize) {
		free(outsize);
	}
	if (spmin) {
		free(spmin);
	}
	if (spmax) {
		free(spmax);
	}
}

/*
 ***************************************************************************
 * Display all graphs for current activity.
 *
 * IN:
 * @g_nr	Number of sets of graphs (views) to display.
 * @g_type	Type of graph (SVG_LINE_GRAPH, SVG_BAR_GRAPH).
 * @title	Titles for each set of graphs.
 * @g_title	Titles for each graph.
 * @item_name	Item (network interface, etc.) name.
 * @group	Indicate how graphs are grouped together to make sets.
 * @spmin	Array containing min values for graphs.
 * @spmax	Array containing max values for graphs.
 * @out		Pointer on array of chars for each graph definition.
 * @outsize	Size of array of chars for each graph definition.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 *		time for the first sample of stats (.@ust_time_first), and
 *		times used as start and end values on the X axis
 *		(.@ust_time_ref and .@ust_time_end).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
void draw_activity_graphs(int g_nr, int g_type, char *title[], char *g_title[], char *item_name,
			  int group[], double *spmin, double *spmax, char **out, int *outsize,
			  struct svg_parm *svg_p, struct record_header *record_hdr)
{
	struct record_header stamp;
	struct tm rectime;
	char *out_p;
	int i, j, dp, pos = 0, views_nr = 0;
	int v_gridnr;
	unsigned int asfactor[16];
	long int k;
	double lmax, xfactor, yfactor, ypos, gmin, gmax;
	char cur_time[32], val[32], stmp[32];

	/* Translate to proper position for current activity */
	printf("<g id=\"g%d\" transform=\"translate(0,%d)\">\n",
	       svg_p->graph_no,
	       SVG_H_YSIZE + svg_p->graph_no * SVG_T_YSIZE);

	/* For each set of graphs which are part of current activity */
	for (i = 0; i < g_nr; i++) {

		/* Get global min and max value for current set of graphs */
		get_global_extrema(pos, group[i], spmin, spmax, &gmin, &gmax);

		/* Don't display empty views if requested */
		if (SKIP_EMPTY_VIEWS(flags) && (gmax < 0.005))
			continue;
		/* Increment number of views actually displayed */
		views_nr++;

		/* Graph background */
		printf("<rect x=\"0\" y=\"%d\" height=\"%d\" width=\"%d\"/>\n",
		       i * SVG_T_YSIZE,
		       SVG_V_YSIZE, SVG_V_XSIZE);

		/* Graph title */
		printf("<text x=\"0\" y=\"%d\" style=\"fill: yellow; stroke: none\">%s",
		       20 + i * SVG_T_YSIZE, title[i]);
		if (item_name) {
			printf(" [%s]", item_name);
		}
		printf("\n");
		printf("<tspan x=\"%d\" y=\"%d\" style=\"fill: yellow; stroke: none; font-size: 12px\">"
		       "(Min, Max values)</tspan>\n</text>\n",
		       5 + SVG_M_XSIZE + SVG_G_XSIZE,
		       25 + i * SVG_T_YSIZE);

		/*
		 * At least two samples are needed.
		 * And a min and max value should have been found.
		 */
		if ((record_hdr->ust_time == svg_p->ust_time_first) ||
		    (*(spmin + pos) == DBL_MAX) || (*(spmax + pos) == -DBL_MIN)) {
			/* No data found */
			printf("<text x=\"0\" y=\"%d\" style=\"fill: red; stroke: none\">No data</text>\n",
			       SVG_M_YSIZE + i * SVG_T_YSIZE);
			continue;
		}

		/* X and Y axis */
		printf("<polyline points=\"%d,%d %d,%d %d,%d\" stroke=\"white\" stroke-width=\"2\"/>\n",
		       SVG_M_XSIZE, SVG_M_YSIZE + i * SVG_T_YSIZE,
		       SVG_M_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE,
		       SVG_M_XSIZE + SVG_G_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE);

		for (j = 0; j < 16; j++) {
			/* Init autoscale factors */
			asfactor[j] = 1;
		}

		if (AUTOSCALE_ON(flags) && (group[i] > 1) && gmax && (g_type == SVG_LINE_GRAPH)) {
			/* Autoscaling... */
			for (j = 0; (j < group[i]) && (j < 16); j++) {
				if (!*(spmax + pos + j) || (*(spmax + pos + j) == gmax))
					continue;

				snprintf(val, 32, "%u", (unsigned int) (gmax / *(spmax + pos + j)));
				if (strlen(val) > 0) {
					asfactor[j] = pwr10(strlen(val) - 1);
				}
			}
		}

		/* Caption */
		for (j = 0; j < group[i]; j++) {
			/* Set dp to TRUE (1) if current metric is based on integer values */
			dp = (g_title[pos + j][0] == '~');
			snprintf(val, 32, "x%u ", asfactor[j]);
			printf("<text x=\"%d\" y=\"%d\" style=\"fill: #%06x; stroke: none; font-size: 12px\">"
			       "%s %s(%.*f, %.*f)</text>\n",
			       5 + SVG_M_XSIZE + SVG_G_XSIZE, SVG_M_YSIZE + i * SVG_T_YSIZE + j * 15,
			       svg_colors[(pos + j) & SVG_COLORS_IDX_MASK], g_title[pos + j] + dp,
			       asfactor[j] == 1 ? "" : val,
			       !dp * 2, *(spmin + pos + j) * asfactor[j],
			       !dp * 2, *(spmax + pos + j) * asfactor[j]);
		}

		/* Translate to proper position for current graph within current activity */
		printf("<g transform=\"translate(%d,%d)\">\n",
		       SVG_M_XSIZE, SVG_M_YSIZE + SVG_G_YSIZE + i * SVG_T_YSIZE);

		/* Grid */
		if (g_type == SVG_LINE_GRAPH) {
			/* For line graphs */
			if (!gmax) {
				/* If all values are zero then set current max value to 1 */
				lmax = 1.0;
			}
			else {
				lmax = gmax;
			}
			/* Max value cannot be too small, else Y graduations will be meaningless */
			if (lmax < SVG_H_GRIDNR * 0.01) {
				lmax = SVG_H_GRIDNR * 0.01;
			}
			ypos = ygrid(lmax, &dp);
		}
		else {
			/* For bar graphs (used for %values) */
			ypos = 25.0; 	/* Draw lines at 25%, 50%, 75% and 100% */
			dp = 0;		/* No decimals */

			/* Max should be always 100% except for percentage values greater than 100% */
			if (gmax > 100.0) {
				lmax = gmax;
			}
			else {
				lmax = 100.0;
			}
		}
		yfactor = (double) -SVG_G_YSIZE / lmax;
		j = 1;
		do {
			printf("<polyline points=\"0,%.2f %d,%.2f\" style=\"vector-effect: non-scaling-stroke; "
			       "stroke: #202020\" transform=\"scale(1,%f)\"/>\n",
			       ypos * j, SVG_G_XSIZE, ypos * j, yfactor);
			j++;
		}
		while (ypos * j <= lmax);
		j = 0;
		do {
			/*
			 * Use same rounded value for graduation numbers as for grid lines
			 * to make sure they are properly aligned.
			 */
			sprintf(stmp, "%.2f", ypos * j);

			printf("<text x=\"0\" y=\"%ld\" style=\"fill: white; stroke: none; font-size: 12px; "
			       "text-anchor: end\">%.*f.</text>\n",
			       (long) (atof(stmp) * yfactor), dp, ypos * j);
			j++;
		}
		while (ypos * j <= lmax);

		/* Set number of vertical lines to 12 when option "oneday" is used */
		v_gridnr = DISPLAY_ONE_DAY(flags) ? 12 : SVG_V_GRIDNR;

		k = xgrid(svg_p->ust_time_ref, svg_p->ust_time_end, v_gridnr);
		xfactor = (double) SVG_G_XSIZE / (svg_p->ust_time_end - svg_p->ust_time_ref);
		stamp.ust_time = svg_p->ust_time_ref; /* Only ust_time field needs to be set. TRUE_TIME not allowed */

		for (j = 0; (j <= v_gridnr) && (stamp.ust_time <= svg_p->ust_time_end); j++) {
			sa_get_record_timestamp_struct(flags, &stamp, &rectime, NULL);
			set_record_timestamp_string(flags, &stamp, NULL, cur_time, 32, &rectime);
			printf("<polyline points=\"%ld,0 %ld,%d\" style=\"vector-effect: non-scaling-stroke; "
			       "stroke: #202020\" transform=\"scale(%f,1)\"/>\n",
			       k * j, k * j, -SVG_G_YSIZE, xfactor);
			/*
			 * NB: We may have tm_min != 0 if we have more than 24H worth of data in one datafile.
			 * In this case, we should rather display the exact time instead of only the hour.
			 */
			if (DISPLAY_ONE_DAY(flags) && (rectime.tm_min == 0)) {
				printf("<text x=\"%ld\" y=\"15\" style=\"fill: white; stroke: none; font-size: 14px; "
				       "text-anchor: start\">%2dH</text>\n",
				       (long) (k * j * xfactor) - 8, rectime.tm_hour);
			}
			else {
				printf("<text x=\"%ld\" y=\"10\" style=\"fill: white; stroke: none; font-size: 12px; "
				       "text-anchor: start\" transform=\"rotate(45,%ld,0)\">%s</text>\n",
				       (long) (k * j * xfactor), (long) (k * j * xfactor), cur_time);
			}
			stamp.ust_time += k;
		}
		if (!PRINT_LOCAL_TIME(flags)) {
			printf("<text x=\"-10\" y=\"30\" style=\"fill: yellow; stroke: none; font-size: 12px; "
			       "text-anchor: end\">UTC</text>\n");
		}

		/* Draw current graphs set */
		for (j = 0; j < group[i]; j++) {
			out_p = *(out + pos + j);
			if (g_type == SVG_LINE_GRAPH) {
				/* Line graphs */
				printf("<path id=\"g%dp%d\" d=\"%s\" "
				       "style=\"vector-effect: non-scaling-stroke; "
				       "stroke: #%06x; stroke-width: 1; fill-opacity: 0\" "
				       "transform=\"scale(%f,%f)\"/>\n",
				       svg_p->graph_no, pos + j, out_p,
				       svg_colors[(pos + j) & SVG_COLORS_IDX_MASK],
				       xfactor,
				       yfactor * asfactor[j]);
			}
			else if (*out_p) {	/* Ignore flat bars */
				/* Bar graphs */
				printf("<g style=\"fill: #%06x; stroke: none\" transform=\"scale(%f,%f)\">\n",
				       svg_colors[(pos + j) & SVG_COLORS_IDX_MASK], xfactor, yfactor);
				printf("%s\n", out_p);
				printf("</g>\n");
			}
			free(out_p);
		}
		printf("</g>\n");
		pos += group[i];
	}
	printf("</g>\n");

	/* Next graph */
	(svg_p->graph_no) += views_nr;
}

/*
 ***************************************************************************
 * Display CPU statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart), and time used for the X axis origin
 *		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_cpu_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				    unsigned long long g_itv, struct record_header *record_hdr)
{
	struct stats_cpu *scc, *scp;
	int group1[] = {5};
	int group2[] = {9};
	char *title[] = {"CPU load"};
	char *g_title1[] = {"%user", "%nice", "%system", "%iowait", "%steal", "%idle"};
	char *g_title2[] = {"%usr", "%nice", "%sys", "%iowait", "%steal", "%irq", "%soft", "%guest", "%gnice", "%idle"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char item_name[8];
	double offset, val;
	int i, j, k, pos, cpu_offline;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(10 * a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* For each CPU */
		for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

			scc = (struct stats_cpu *) ((char *) a->buf[curr]  + i * a->msize);
			scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

			/* Should current CPU (including CPU "all") be displayed? */
			if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
				/* No */
				continue;

			pos = i * 10;
			offset = 0.0;

			if (i) {	/* Don't test CPU "all" here */
				/*
				 * If the CPU is offline then it is omited from /proc/stat:
				 * All the fields couldn't have been read and the sum of them is zero.
				 * (Remember that guest/guest_nice times are already included in
				 * user/nice modes.)
				 */
				if ((scc->cpu_user    + scc->cpu_nice + scc->cpu_sys   +
				     scc->cpu_iowait  + scc->cpu_idle + scc->cpu_steal +
				     scc->cpu_hardirq + scc->cpu_softirq) == 0) {
					/*
					 * Set current struct fields (which have been set to zero)
					 * to values from previous iteration. Hence their values won't
					 * jump from zero when the CPU comes back online.
					 */
					*scc = *scp;

					g_itv = 0;
					cpu_offline = TRUE;
				}
				else {
					/*
					 * Recalculate interval for current proc.
					 * If result is 0 then current CPU is a tickless one.
					 */
					g_itv = get_per_cpu_interval(scc, scp);
					cpu_offline = FALSE;
				}

				if (!g_itv) {	/* Current CPU is offline or tickless */

					val = (cpu_offline ? 0.0	/* Offline CPU: %idle = 0% */
							   : 100.0);	/* Tickless CPU: %idle = 100% */

					if (DISPLAY_CPU_DEF(a->opt_flags)) {
						j  = 5;	/* -u */
					}
					else {	/* DISPLAY_CPU_ALL(a->opt_flags) */
						j = 9;	/* -u ALL */
					}

					/* Check min/max values for %user, etc. */
					for (k = 0; k < j; k++) {
						if (0.0 < *(spmin + pos + k)) {
							*(spmin + pos + k) = 0.0;
						}
						if (0.0 > *(spmax + pos + k)) {
							*(spmax + pos + k) = 0.0;
						}
					}

					/* %idle */
					cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
						  &offset, val,
						  out + pos + j, outsize + pos + j, svg_p->dt,
						  spmin + pos + j, spmax + pos + j);
					continue;
				}
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				/* %user */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_user, scc->cpu_user, g_itv),
					  out + pos, outsize + pos, svg_p->dt,
					  spmin + pos, spmax + pos);
			}
			else {
				/* %usr */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset,
					  (scc->cpu_user - scc->cpu_guest) < (scp->cpu_user - scp->cpu_guest) ?
					   0.0 :
					   ll_sp_value(scp->cpu_user - scp->cpu_guest,
						       scc->cpu_user - scc->cpu_guest, g_itv),
					  out + pos, outsize + pos, svg_p->dt,
					  spmin + pos, spmax + pos);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				/* %nice */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_nice, scc->cpu_nice, g_itv),
					  out + pos + 1, outsize + pos + 1, svg_p->dt,
					  spmin + pos + 1, spmax + pos + 1);
			}
			else {
				/* %nice */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset,
					  (scc->cpu_nice - scc->cpu_guest_nice) < (scp->cpu_nice - scp->cpu_guest_nice) ?
					   0.0 :
					   ll_sp_value(scp->cpu_nice - scp->cpu_guest_nice,
						       scc->cpu_nice - scc->cpu_guest_nice, g_itv),
					  out + pos + 1, outsize + pos + 1, svg_p->dt,
					  spmin + pos + 1, spmax + pos + 1);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				/* %system */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset,
					  ll_sp_value(scp->cpu_sys + scp->cpu_hardirq + scp->cpu_softirq,
						      scc->cpu_sys + scc->cpu_hardirq + scc->cpu_softirq,
						      g_itv),
					  out + pos + 2, outsize + pos + 2, svg_p->dt,
					  spmin + pos + 2, spmax + pos + 2);
			}
			else {
				/* %sys */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_sys, scc->cpu_sys, g_itv),
					  out + pos + 2, outsize + pos + 2, svg_p->dt,
					  spmin + pos + 2, spmax + pos + 2);
			}

			/* %iowait */
			cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
				  &offset, ll_sp_value(scp->cpu_iowait, scc->cpu_iowait, g_itv),
				  out + pos + 3, outsize + pos + 3, svg_p->dt,
				  spmin + pos + 3, spmax + pos + 3);

			/* %steal */
			cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
				  &offset, ll_sp_value(scp->cpu_steal, scc->cpu_steal, g_itv),
				  out + pos + 4, outsize + pos + 4, svg_p->dt,
				  spmin + pos + 4, spmax + pos + 4);

			if (DISPLAY_CPU_ALL(a->opt_flags)) {
				/* %irq */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_hardirq, scc->cpu_hardirq, g_itv),
					  out + pos + 5, outsize + pos + 5, svg_p->dt,
					  spmin + pos + 5, spmax + pos + 5);

				/* %soft */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_softirq, scc->cpu_softirq, g_itv),
					  out + pos + 6, outsize + pos + 6, svg_p->dt,
					  spmin + pos + 6, spmax + pos + 6);

				/* %guest */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_guest, scc->cpu_guest, g_itv),
					  out + pos + 7, outsize + pos + 7, svg_p->dt,
					  spmin + pos + 7, spmax + pos + 7);

				/* %gnice */
				cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
					  &offset, ll_sp_value(scp->cpu_guest_nice, scc->cpu_guest_nice, g_itv),
					  out + pos + 8, outsize + pos + 8, svg_p->dt,
					  spmin + pos + 8, spmax + pos + 8);

				j = 9;
			}
			else {
				j = 5;
			}

			/* %idle */
			cpuappend(record_hdr->ust_time - svg_p->ust_time_ref,
				  &offset,
				  (scc->cpu_idle < scp->cpu_idle ? 0.0 :
				   ll_sp_value(scp->cpu_idle, scc->cpu_idle, g_itv)),
				  out + pos + j, outsize + pos + j, svg_p->dt,
				  spmin + pos + j, spmax + pos + j);
		}
	}

	if (action & F_END) {
		for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

			/* Should current CPU (including CPU "all") be displayed? */
			if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
				/* No */
				continue;

			pos = i * 10;
			if (!i) {
				/* This is CPU "all" */
				strcpy(item_name, "all");
			}
			else {
				sprintf(item_name, "%d", i - 1);
			}

			if (DISPLAY_CPU_DEF(a->opt_flags)) {
				draw_activity_graphs(a->g_nr, SVG_BAR_GRAPH,
						     title, g_title1, item_name, group1,
						     spmin + pos, spmax + pos, out + pos, outsize + pos,
						     svg_p, record_hdr);
			}
			else {
				draw_activity_graphs(a->g_nr, SVG_BAR_GRAPH,
						     title, g_title2, item_name, group2,
						     spmin + pos, spmax + pos, out + pos, outsize + pos,
						     svg_p, record_hdr);
			}
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display task creation and context switch statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pcsw_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				     unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr],
		*spp = (struct stats_pcsw *) a->buf[!curr];
	int group[] = {1, 1};
	char *title[] = {"Switching activity", "Task creation"};
	char *g_title[] = {"cswch/s",
			   "proc/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(2, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(1, 1, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);
		/* cswch/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->context_switch, spc->context_switch, itv),
			 out, outsize, svg_p->restart);
		/* proc/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->processes, spc->processes, itv),
			 out + 1, outsize + 1, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display swap statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_swap_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				     unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr],
		*ssp = (struct stats_swap *) a->buf[!curr];
	int group[] = {2};
	char *title[] = {"Swap activity"};
	char *g_title[] = {"pswpin/s", "pswpout/s" };
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(2, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 2, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);
		/* pswpin/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(ssp->pswpin, ssc->pswpin, itv),
			 out, outsize, svg_p->restart);
		/* pswpout/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(ssp->pswpout, ssc->pswpout, itv),
			 out + 1, outsize + 1, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display paging statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_paging_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				       unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr],
		*spp = (struct stats_paging *) a->buf[!curr];
	int group[] = {2, 2, 4};
	char *title[] = {"Paging activity (1)", "Paging activity (2)", "Paging activity (3)"};
	char *g_title[] = {"pgpgin/s", "pgpgout/s",
			   "fault/s", "majflt/s",
			   "pgfree/s", "pgscank/s", "pgscand/s", "pgsteal/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(8, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 8, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);
		/* pgpgin/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgpgin, spc->pgpgin, itv),
			 out, outsize, svg_p->restart);
		/* pgpgout/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgpgout, spc->pgpgout, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* fault/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgfault, spc->pgfault, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* majflt/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgmajfault, spc->pgmajfault, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* pgfree/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgfree, spc->pgfree, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* pgscank/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgscan_kswapd, spc->pgscan_kswapd, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* pgscand/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgscan_direct, spc->pgscan_direct, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* pgsteal/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(spp->pgsteal, spc->pgsteal, itv),
			 out + 7, outsize + 7, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display I/O and transfer rate statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_io_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				   unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr],
		*sip = (struct stats_io *) a->buf[!curr];
	int group[] = {3, 2};
	char *title[] = {"I/O and transfer rate statistics (1)", "I/O and transfer rate statistics (2)"};
	char *g_title[] = {"tps", "rtps", "wtps",
			   "bread/s", "bwrtn/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(5, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 5, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* tps */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sip->dk_drive, sic->dk_drive, itv),
			 out, outsize, svg_p->restart);
		/* rtps */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sip->dk_drive_rio,  sic->dk_drive_rio, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* wtps */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sip->dk_drive_wio,  sic->dk_drive_wio, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* bread/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sip->dk_drive_rblk, sic->dk_drive_rblk, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* bwrtn/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sip->dk_drive_wblk, sic->dk_drive_wblk, itv),
			 out + 4, outsize + 4, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display memory statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_memory_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				       unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];
	int group1a[] = {2, 2};
	int group1b[] = {1, 1};
	int group1c[] = {4, 5};
	int group2a[] = {3};
	int group2b[] = {1, 1};
	char *title1a[] = {"Memory utilization (1)", "Memory utilization (2)"};
	char *title1b[] = {"Memory utilization (3)", "Memory utilization (4)"};
	char *title1c[] = {"Memory utilization (5)", "Memory utilization (6)"};
	char *title2a[] = {"Swap utilization (1)"};
	char *title2b[] = {"Swap utilization (2)", "Swap utilization (3)"};
	char *g_title1a[] = {"MBmemfree", "MBmemused",
			     "MBcached", "MBbuffers"};
	char *g_title1b[] = {"%memused", "%commit"};
	char *g_title1c[] = {"MBcommit", "MBactive", "MBinact", "MBdirty",
			     "MBanonpg", "MBslab", "MBkstack", "MBpgtbl", "MBvmused"};
	char *g_title2a[] = {"MBswpfree", "MBswpused", "MBswpcad"};
	char *g_title2b[] = {"%swpused", "%swpcad"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	double tval;
	int i;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(22, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 16, 0, (void *) a->buf[curr], NULL,
			     0, spmin, spmax);
		/* Compute %memused min/max values */
		tval = smc->tlmkb ? SP_VALUE(smc->frmkb, smc->tlmkb, smc->tlmkb) : 0.0;
		if (tval > *(spmax + 16)) {
			*(spmax + 16) = tval;
		}
		if (tval < *(spmin + 16)) {
			*(spmin + 16) = tval;
		}
		/* Compute %commit min/max values */
		tval = (smc->tlmkb + smc->tlskb) ?
		       SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb) : 0.0;
		if (tval > *(spmax + 17)) {
			*(spmax + 17) = tval;
		}
		if (tval < *(spmin + 17)) {
			*(spmin + 17) = tval;
		}
		/* Compute %swpused min/max values */
		tval = smc->tlskb ?
		       SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb) : 0.0;
		if (tval > *(spmax + 18)) {
			*(spmax + 18) = tval;
		}
		if (tval < *(spmin + 18)) {
			*(spmin + 18) = tval;
		}
		/* Compute %swpcad min/max values */
		tval = (smc->tlskb - smc->frskb) ?
		       SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb) : 0.0;
		if (tval > *(spmax + 19)) {
			*(spmax + 19) = tval;
		}
		if (tval < *(spmin + 19)) {
			*(spmin + 19) = tval;
		}
		/* Compute memused min/max values in MB */
		tval = ((double) (smc->tlmkb - smc->frmkb)) / 1024;
		if (tval > *(spmax + 20)) {
			*(spmax + 20) = tval;
		}
		if (tval < *(spmin + 20)) {
			*(spmin + 20) = tval;
		}
		/* Compute swpused min/max values in MB */
		tval = ((double) (smc->tlskb - smc->frskb)) / 1024;
		if (tval > *(spmax + 21)) {
			*(spmax + 21) = tval;
		}
		if (tval < *(spmin + 21)) {
			*(spmin + 21) = tval;
		}

		/* MBmemfree */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->frmkb) / 1024,
			 out, outsize, svg_p->restart);
		/* MBmemused */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) (smc->tlmkb - smc->frmkb)) / 1024,
			 out + 1, outsize + 1, svg_p->restart);
		/* MBcached */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->camkb) / 1024,
			  out + 2, outsize + 2, svg_p->restart);
		/* MBbuffers */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->bufkb) / 1024,
			 out + 3, outsize + 3, svg_p->restart);
		/* MBswpfree */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->frskb) / 1024,
			 out + 4, outsize + 4, svg_p->restart);
		/* MBswpused */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) (smc->tlskb - smc->frskb)) / 1024,
			 out + 5, outsize + 5, svg_p->restart);
		/* MBswpcad */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->caskb) / 1024,
			 out + 6, outsize + 6, svg_p->restart);
		/* MBcommit */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->comkb) / 1024,
			 out + 7, outsize + 7, svg_p->restart);
		/* MBactive */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->activekb) / 1024,
			 out + 8, outsize + 8, svg_p->restart);
		/* MBinact */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->inactkb) / 1024,
			 out + 9, outsize + 9, svg_p->restart);
		/* MBdirty */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->dirtykb) / 1024,
			 out + 10, outsize + 10, svg_p->restart);
		/* MBanonpg */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->anonpgkb) / 1024,
			 out + 11, outsize + 11, svg_p->restart);
		/* MBslab */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->slabkb) / 1024,
			 out + 12, outsize + 12, svg_p->restart);
		/* MBkstack */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->kstackkb) / 1024,
			 out + 13, outsize + 13, svg_p->restart);
		/* MBpgtbl */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->pgtblkb) / 1024,
			 out + 14, outsize + 14, svg_p->restart);
		/* MBvmused */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 ((double) smc->vmusedkb) / 1024,
			 out + 15, outsize + 15, svg_p->restart);
		/* %memused */
		brappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 0.0,
			 smc->tlmkb ?
			 SP_VALUE(smc->frmkb, smc->tlmkb, smc->tlmkb) : 0.0,
			 out + 16, outsize + 16, svg_p->dt);
		/* %commit */
		brappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 0.0,
			 (smc->tlmkb + smc->tlskb) ?
			 SP_VALUE(0, smc->comkb, smc->tlmkb + smc->tlskb) : 0.0,
			 out + 17, outsize + 17, svg_p->dt);
		/* %swpused */
		brappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 0.0,
			 smc->tlskb ?
			 SP_VALUE(smc->frskb, smc->tlskb, smc->tlskb) : 0.0,
			 out + 18, outsize + 18, svg_p->dt);
		/* %swpcad */
		brappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 0.0,
			 (smc->tlskb - smc->frskb) ?
			 SP_VALUE(0, smc->caskb, smc->tlskb - smc->frskb) : 0.0,
			 out + 19, outsize + 19, svg_p->dt);
	}

	if (action & F_END) {

		/* Conversion kB -> MB */
		for (i = 0; i < 16; i++) {
			*(spmin + i) /= 1024;
			*(spmax + i) /= 1024;
		}

		if (DISPLAY_MEM_AMT(a->opt_flags)) {
			/* frmkb and tlmkb should be together because they will be drawn on the same view */
			*(spmax + 3) = *(spmax + 1);
			*(spmin + 3) = *(spmin + 1);
			/* Move memused min/max values */
			*(spmax + 1) = *(spmax + 20);
			*(spmin + 1) = *(spmin + 20);

			draw_activity_graphs(2, SVG_LINE_GRAPH, title1a, g_title1a, NULL, group1a,
					     spmin, spmax, out, outsize, svg_p, record_hdr);
			draw_activity_graphs(2, SVG_BAR_GRAPH, title1b, g_title1b, NULL, group1b,
					     spmin + 16, spmax + 16, out + 16, outsize + 16, svg_p, record_hdr);
			draw_activity_graphs(DISPLAY_MEM_ALL(a->opt_flags) ? 2 : 1,
					     SVG_LINE_GRAPH, title1c, g_title1c, NULL, group1c,
					     spmin + 7, spmax + 7, out + 7, outsize + 7, svg_p, record_hdr);
		}

		if (DISPLAY_SWAP(a->opt_flags)) {
			/* Move swpused min/max values */
			*(spmax + 5) = *(spmax + 21);
			*(spmin + 5) = *(spmin + 21);

			draw_activity_graphs(1, SVG_LINE_GRAPH, title2a, g_title2a, NULL, group2a,
					     spmin + 4, spmax + 4, out + 4, outsize + 4, svg_p, record_hdr);
			draw_activity_graphs(2, SVG_BAR_GRAPH, title2b, g_title2b, NULL, group2b,
					     spmin + 18, spmax + 18, out + 18, outsize + 18, svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display kernel tables statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_ktables_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];
	int group[] = {3, 1};
	char *title[] = {"Kernel tables (1)", "Kernel tables (2)"};
	char *g_title[] = {"~file-nr", "~inode-nr", "~dentunusd",
			   "~pty-nr"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(4, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 0, 4, (void *) a->buf[curr], NULL,
			     itv, spmin, spmax);
		/* file-nr */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) skc->file_used,
			  out, outsize, svg_p->restart);
		/* inode-nr */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) skc->inode_used,
			  out + 1, outsize + 1, svg_p->restart);
		/* dentunusd */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) skc->dentry_stat,
			  out + 2, outsize + 2, svg_p->restart);
		/* pty-nr */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) skc->pty_nr,
			  out + 3, outsize + 3, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display queue and load statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_queue_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				      unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];
	int group[] = {2, 3, 1};
	char *title[] = {"Queue length", "Load average", "Task list"};
	char *g_title[] = {"~runq-sz", "~blocked",
			   "ldavg-1", "ldavg-5", "ldavg-15",
			   "~plist-sz"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(6, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 2, 4, (void *) a->buf[curr], NULL,
			     itv, spmin, spmax);
		/* runq-sz */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) sqc->nr_running,
			  out, outsize, svg_p->restart);
		/* blocked */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) sqc->procs_blocked,
			  out + 1, outsize + 1, svg_p->restart);
		/* ldavg-1 */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 (double) sqc->load_avg_1 / 100,
			 out + 2, outsize + 2, svg_p->restart);
		/* ldavg-5 */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 (double) sqc->load_avg_5 / 100,
			 out + 3, outsize + 3, svg_p->restart);
		/* ldavg-15 */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 (double) sqc->load_avg_15 / 100,
			 out + 4, outsize + 4, svg_p->restart);
		/* plist-sz */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) sqc->nr_threads,
			  out + 5, outsize + 5, svg_p->restart);
	}

	if (action & F_END) {
		/* Fix min/max values for load average */
		*(spmin + 2) /= 100; *(spmax + 2) /= 100;
		*(spmin + 3) /= 100; *(spmax + 3) /= 100;
		*(spmin + 4) /= 100; *(spmax + 4) /= 100;

		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display network interfaces statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_dev_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_dev *sndc, *sndp;
	int group1[] = {2, 2, 3};
	int group2[] = {1};
	char *title1[] = {"Network statistics (1)", "Network statistics (2)",
			  "Network statistics (3)"};
	char *title2[] = {"Network statistics (4)"};
	char *g_title1[] = {"rxpck/s", "txpck/s",
			    "rxkB/s", "txkB/s",
			    "rxcmp/s", "txcmp/s", "rxmcst/s"};
	char *g_title2[] = {"%ifutil"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char *item_name;
	double rxkb, txkb, ifutil;
	int i, j, k, pos, restart, *unregistered;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays (#0..7) that will contain the graphs data
		 * and the min/max values.
		 * Also allocate one additional array (#8) for each interface:
		 * out + 8 will contain the interface name,
		 * outsize + 8 will contain a positive value (TRUE) if the interface
		 * has either still not been registered, or has been unregistered.
		 */
		out = allocate_graph_lines(9 * a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		restart = svg_p->restart;
		/*
		 * Mark previously registered interfaces as now
		 * possibly unregistered for all graphs.
		 */
		for (k = 0; k < a->nr; k++) {
			unregistered = outsize + k * 9 + 8;
			if (*unregistered == FALSE) {
				*unregistered = MAYBE;
			}
		}

		/* For each network interfaces structure */
		for (i = 0; i < a->nr; i++) {
			sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);
			if (!strcmp(sndc->interface, ""))
				/* Empty structure: Ignore it */
				continue;

			/* Look for corresponding graph */
			for (k = 0; k < a->nr; k++) {
				item_name = *(out + k * 9 + 8);
				if (!strcmp(sndc->interface, item_name))
					/* Graph found! */
					break;
			}
			if (k == a->nr) {
				/* Graph not found: Look for first free entry */
				for (k = 0; k < a->nr; k++) {
					item_name = *(out + k * 9 + 8);
					if (!strcmp(item_name, ""))
						break;
				}
				if (k == a->nr)
					/* No free graph entry: Graph for this item won't be drawn */
					continue;
			}

			pos = k * 9;
			unregistered = outsize + pos + 8;

			j = check_net_dev_reg(a, curr, !curr, i);
			sndp = (struct stats_net_dev *) ((char *) a->buf[!curr] + j * a->msize);

			/*
			 * If current interface was marked as previously unregistered,
			 * then set restart variable to TRUE so that the graph will be
			 * discontinuous, and mark it as now registered.
			 */
			if (*unregistered == TRUE) {
				restart = TRUE;
			}
			*unregistered = FALSE;

			if (!item_name[0]) {
				/* Save network interface name (if not already done) */
				strncpy(item_name, sndc->interface, CHUNKSIZE);
				item_name[CHUNKSIZE - 1] = '\0';
			}

			/* Check for min/max values */
			save_extrema(7, 0, 0, (void *) sndc, (void *) sndp,
				     itv, spmin + pos, spmax + pos);

			rxkb = S_VALUE(sndp->rx_bytes, sndc->rx_bytes, itv);
			txkb = S_VALUE(sndp->tx_bytes, sndc->tx_bytes, itv);
			ifutil = compute_ifutil(sndc, rxkb, txkb);
			if (ifutil < *(spmin + pos + 7)) {
				*(spmin + pos + 7) = ifutil;
			}
			if (ifutil > *(spmax + pos + 7)) {
				*(spmax + pos + 7) = ifutil;
			}

			/* rxpck/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(sndp->rx_packets, sndc->rx_packets, itv),
				 out + pos, outsize + pos, restart);

			/* txpck/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(sndp->tx_packets, sndc->tx_packets, itv),
				 out + pos + 1, outsize + pos + 1, restart);

			/* rxkB/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 rxkb / 1024,
				 out + pos + 2, outsize + pos + 2, restart);

			/* txkB/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 txkb / 1024,
				 out + pos + 3, outsize + pos + 3, restart);

			/* rxcmp/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(sndp->rx_compressed, sndc->rx_compressed, itv),
				 out + pos + 4, outsize + pos + 4, restart);

			/* txcmp/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(sndp->tx_compressed, sndc->tx_compressed, itv),
				 out + pos + 5, outsize + pos + 5, restart);

			/* rxmcst/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(sndp->multicast, sndc->multicast, itv),
				 out + pos + 6, outsize + pos + 6, restart);

			/* %ifutil */
			brappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 0.0, ifutil,
				 out + pos + 7, outsize + pos + 7, svg_p->dt);
		}

		/* Mark interfaces not seen here as now unregistered */
		for (k = 0; k < a->nr; k++) {
			unregistered = outsize + k * 9 + 8;
			if (*unregistered != FALSE) {
				*unregistered = TRUE;
			}
		}
	}

	if (action & F_END) {
		for (i = 0; i < a->nr; i++) {
			/*
			 * Check if there is something to display.
			 * Don't test sndc->interface because maybe the network
			 * interface has been registered later.
			 */
			pos = i * 9;
			if (!**(out + pos))
				continue;

			/* Recalculate min and max values in kB, not in B */
			*(spmin + pos + 2) /= 1024;
			*(spmax + pos + 2) /= 1024;
			*(spmin + pos + 3) /= 1024;
			*(spmax + pos + 3) /= 1024;

			item_name = *(out + pos + 8);
			draw_activity_graphs(a->g_nr - 1, SVG_LINE_GRAPH,
					     title1, g_title1, item_name, group1,
					     spmin + pos, spmax + pos, out + pos, outsize + pos,
					     svg_p, record_hdr);
			draw_activity_graphs(1, SVG_BAR_GRAPH,
					     title2, g_title2, item_name, group2,
					     spmin + pos + 7, spmax + pos + 7, out + pos + 7, outsize + pos + 7,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display network interface errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_edev_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_edev *snedc, *snedp;
	int group[] = {2, 2, 2, 3};
	char *title[] = {"Network statistics (1)", "Network statistics (2)",
			 "Network statistics (3)", "Network statistics (4)"};
	char *g_title[] = {"rxerr/s", "txerr/s",
			    "rxdrop/s", "txdrop/s",
			    "rxfifo/s", "txfifo/s",
			    "rxfram/s", "txcarr/s", "coll/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char *item_name;
	double tmpmin, tmpmax;
	int i, j, k, pos, restart, *unregistered;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays (#0..8) that will contain the graphs data
		 * and the min/max values.
		 * Also allocate one additional array (#9) for each interface:
		 * out + 9 will contain the interface name,
		 * outsize + 9 will contain a positive value (TRUE) if the interface
		 * has either still not been registered, or has been unregistered.
		 */
		out = allocate_graph_lines(10 * a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		restart = svg_p->restart;
		/*
		 * Mark previously registered interfaces as now
		 * possibly unregistered for all graphs.
		 */
		for (k = 0; k < a->nr; k++) {
			unregistered = outsize + k * 10 + 9;
			if (*unregistered == FALSE) {
				*unregistered = MAYBE;
			}
		}

		/* For each network interfaces structure */
		for (i = 0; i < a->nr; i++) {
			snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);
			if (!strcmp(snedc->interface, ""))
				/* Empty structure: Ignore it */
				continue;

			/* Look for corresponding graph */
			for (k = 0; k < a->nr; k++) {
				item_name = *(out + k * 10 + 9);
				if (!strcmp(snedc->interface, item_name))
					/* Graph found! */
					break;
			}
			if (k == a->nr) {
				/* Graph not found: Look for first free entry */
				for (k = 0; k < a->nr; k++) {
					item_name = *(out + k * 10 + 9);
					if (!strcmp(item_name, ""))
						break;
				}
				if (k == a->nr)
					/* No free graph entry: Graph for this item won't be drawn */
					continue;
			}

			pos = k * 10;
			unregistered = outsize + pos + 9;

			j = check_net_edev_reg(a, curr, !curr, i);
			snedp = (struct stats_net_edev *) ((char *) a->buf[!curr] + j * a->msize);

			/*
			 * If current interface was marked as previously unregistered,
			 * then set restart variable to TRUE so that the graph will be
			 * discontinuous, and mark it as now registered.
			 */
			if (*unregistered == TRUE) {
				restart = TRUE;
			}
			*unregistered = FALSE;

			if (!item_name[0]) {
				/* Save network interface name (if not already done) */
				strncpy(item_name, snedc->interface, CHUNKSIZE);
				item_name[CHUNKSIZE - 1] = '\0';
			}

			/* Check for min/max values */
			save_extrema(9, 0, 0, (void *) snedc, (void *) snedp,
				     itv, spmin + pos, spmax + pos);

			/* rxerr/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->rx_errors, snedc->rx_errors, itv),
				 out + pos, outsize + pos, restart);

			/* txerr/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->tx_errors, snedc->tx_errors, itv),
				 out + pos + 1, outsize + pos + 1, restart);

			/* rxdrop/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->rx_dropped, snedc->rx_dropped, itv),
				 out + pos + 2, outsize + pos + 2, restart);

			/* txdrop/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->tx_dropped, snedc->tx_dropped, itv),
				 out + pos + 3, outsize + pos + 3, restart);

			/* rxfifo/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->rx_fifo_errors, snedc->rx_fifo_errors, itv),
				 out + pos + 4, outsize + pos + 4, restart);

			/* txfifo/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->tx_fifo_errors, snedc->tx_fifo_errors, itv),
				 out + pos + 5, outsize + pos + 5, restart);

			/* rxfram/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->rx_frame_errors, snedc->rx_frame_errors, itv),
				 out + pos + 6, outsize + pos + 6, restart);

			/* txcarr/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->tx_carrier_errors, snedc->tx_carrier_errors, itv),
				 out + pos + 7, outsize + pos + 7, restart);

			/* coll/s */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 S_VALUE(snedp->collisions, snedc->collisions, itv),
				 out + pos + 8, outsize + pos + 8, restart);
		}

		/* Mark interfaces not seen here as now unregistered */
		for (k = 0; k < a->nr; k++) {
			unregistered = outsize + k * 10 + 9;
			if (*unregistered != FALSE) {
				*unregistered = TRUE;
			}
		}
	}

	if (action & F_END) {
		for (i = 0; i < a->nr; i++) {
			/*
			 * Check if there is something to display.
			 * Don't test snedc->interface because maybe the network
			 * interface has been registered later.
			 */
			pos = i * 10;
			if (!**(out + pos))
				continue;

			/*
			 * Move coll/s min and max values at the end of the list,
			 * because coll/s graph will be drawn on the last view.
			 */
			tmpmin = *(spmin + pos);
			tmpmax = *(spmax + pos);
			for (k = 1; k < 9; k++) {
				*(spmin + pos + k - 1) = *(spmin + pos + k);
				*(spmax + pos + k - 1) = *(spmax + pos + k);
			}
			*(spmin + pos + 8) = tmpmin;
			*(spmax + pos + 8) = tmpmax;

			item_name = *(out + pos + 9);
			draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH,
					     title, g_title, item_name, group,
					     spmin + pos, spmax + pos, out + pos, outsize + pos,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display NFS client statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_nfs_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr],
		*snnp = (struct stats_net_nfs *) a->buf[!curr];
	int group[] = {2, 2, 2};
	char *title[] = {"NFS client statistics (1)", "NFS client statistics (2)", "NFS client statistics (3)"};
	char *g_title[] = {"call/s", "retrans/s",
			   "read/s", "write/s",
			   "access/s", "getatt/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(6, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 0, 6, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* call/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_rpccnt, snnc->nfs_rpccnt, itv),
			 out, outsize, svg_p->restart);
		/* retrans/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_rpcretrans, snnc->nfs_rpcretrans, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* read/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_readcnt, snnc->nfs_readcnt, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* write/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_writecnt, snnc->nfs_writecnt, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* access/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_accesscnt, snnc->nfs_accesscnt, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* getatt/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snnp->nfs_getattcnt, snnc->nfs_getattcnt, itv),
			 out + 5, outsize + 5, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display network socket statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_sock_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];
	int group[] = {1, 5};
	char *title[] = {"Network sockets (1)", "Network sockets (2)"};
	char *g_title[] = {"~totsck",
			   "~tcpsck", "~tcp-tw", "~udpsck", "~rawsck", "~ip-frag"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(6, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 0, 6, (void *) a->buf[curr], NULL,
			     itv, spmin, spmax);
		/* totsck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->sock_inuse,
			  out, outsize, svg_p->restart);
		/* tcpsck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->tcp_inuse,
			  out + 1, outsize + 1, svg_p->restart);
		/* tcp-tw */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->tcp_tw,
			  out + 2, outsize + 2, svg_p->restart);
		/* udpsck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->udp_inuse,
			  out + 3, outsize + 3, svg_p->restart);
		/* rawsck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->raw_inuse,
			  out + 4, outsize + 4, svg_p->restart);
		/* ip-frag */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->frag_inuse,
			  out + 5, outsize + 5, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display IPv4 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_ip_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				       unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr],
		*snip = (struct stats_net_ip *) a->buf[!curr];
	int group[] = {4, 2, 2};
	char *title[] = {"IPv4 network statistics (1)", "IPv4 network statistics (2)", "IPv4 network statistics (3)"};
	char *g_title[] = {"irec/s", "fwddgm/s", "idel/s", "orq/s",
			   "asmrq/s", "asmok/s",
			   "fragok/s", "fragcrt/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(8, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(8, 0, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* irec/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InReceives, snic->InReceives, itv),
			 out, outsize, svg_p->restart);
		/* fwddgm/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->ForwDatagrams, snic->ForwDatagrams, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* idel/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InDelivers, snic->InDelivers, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* orq/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutRequests, snic->OutRequests, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* asmrq/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->ReasmReqds, snic->ReasmReqds, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* asmok/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->ReasmOKs, snic->ReasmOKs, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* fragok/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->FragOKs, snic->FragOKs, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* fragcrt/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->FragCreates, snic->FragCreates, itv),
			 out + 7, outsize + 7, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display IPv4 network errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_eip_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr],
		*sneip = (struct stats_net_eip *) a->buf[!curr];
	int group[] = {3, 2, 3};
	char *title[] = {"IPv4 network errors statistics (1)", "IPv4 network errors statistics (2)",
			 "IPv4 network errors statistics (3)"};
	char *g_title[] = {"ihdrerr/s", "iadrerr/s", "iukwnpr/s",
			   "idisc/s", "odisc/s",
			   "onort/s", "asmf/s", "fragf/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(8, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(8, 0, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* ihdrerr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InHdrErrors, sneic->InHdrErrors, itv),
			 out, outsize, svg_p->restart);
		/* iadrerr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InAddrErrors, sneic->InAddrErrors, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* iukwnpr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InUnknownProtos, sneic->InUnknownProtos, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* idisc/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InDiscards, sneic->InDiscards, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* odisc/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutDiscards, sneic->OutDiscards, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* onort/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutNoRoutes, sneic->OutNoRoutes, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* asmf/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->ReasmFails, sneic->ReasmFails, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* fragf/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->FragFails, sneic->FragFails, itv),
			 out + 7, outsize + 7, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display ICMPv4 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_icmp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr],
		*snip = (struct stats_net_icmp *) a->buf[!curr];
	int group[] = {2, 4, 4, 4};
	char *title[] = {"ICMPv4 network statistics (1)", "ICMPv4 network statistics (2)",
			 "ICMPv4 network statistics (3)", "ICMPv4 network statistics (4)"};
	char *g_title[] = {"imsg/s", "omsg/s",
			   "iech/s", "iechr/s", "oech/s", "oechr/s",
			   "itm/s", "itmr/s", "otm/s", "otmr/s",
			   "iadrmk/s", "iadrmkr/s", "oadrmk/s", "oadrmkr/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(14, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 14, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* imsg/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InMsgs, snic->InMsgs, itv),
			 out, outsize, svg_p->restart);
		/* omsg/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutMsgs, snic->OutMsgs, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* iech/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InEchos, snic->InEchos, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* iechr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InEchoReps, snic->InEchoReps, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* oech/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutEchos, snic->OutEchos, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* oechr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutEchoReps, snic->OutEchoReps, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* itm/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InTimestamps, snic->InTimestamps, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* itmr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InTimestampReps, snic->InTimestampReps, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* otm/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutTimestamps, snic->OutTimestamps, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* otmr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutTimestampReps, snic->OutTimestampReps, itv),
			 out + 9, outsize + 9, svg_p->restart);
		/* iadrmk/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InAddrMasks, snic->InAddrMasks, itv),
			 out + 10, outsize + 10, svg_p->restart);
		/* iadrmkr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InAddrMaskReps, snic->InAddrMaskReps, itv),
			 out + 11, outsize + 11, svg_p->restart);
		/* oadrmk/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutAddrMasks, snic->OutAddrMasks, itv),
			 out + 12, outsize + 12, svg_p->restart);
		/* oadrmkr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutAddrMaskReps, snic->OutAddrMaskReps, itv),
			 out + 13, outsize + 13, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display ICMPv4 network errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_eicmp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					  unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr],
		*sneip = (struct stats_net_eicmp *) a->buf[!curr];
	int group[] = {2, 2, 2, 2, 2, 2};
	char *title[] = {"ICMPv4 network errors statistics (1)", "ICMPv4 network errors statistics (2)",
			 "ICMPv4 network errors statistics (3)", "ICMPv4 network errors statistics (4)",
			 "ICMPv4 network errors statistics (5)", "ICMPv4 network errors statistics (6)"};
	char *g_title[] = {"ierr/s", "oerr/s",
			   "idstunr/s", "odstunr/s",
			   "itmex/s", "otmex/s",
			   "iparmpb/s", "oparmpb/s",
			   "isrcq/s", "osrcq/s",
			   "iredir/s", "oredir/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(12, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 12, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* ierr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InErrors, sneic->InErrors, itv),
			 out, outsize, svg_p->restart);
		/* oerr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutErrors, sneic->OutErrors, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* idstunr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InDestUnreachs, sneic->InDestUnreachs, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* odstunr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutDestUnreachs, sneic->OutDestUnreachs, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* itmex/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InTimeExcds, sneic->InTimeExcds, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* otmex/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutTimeExcds, sneic->OutTimeExcds, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* iparmpb/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InParmProbs, sneic->InParmProbs, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* oparmpb/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutParmProbs, sneic->OutParmProbs, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* isrcq/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InSrcQuenchs, sneic->InSrcQuenchs, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* osrcq/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutSrcQuenchs, sneic->OutSrcQuenchs, itv),
			 out + 9, outsize + 9, svg_p->restart);
		/* iredir/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InRedirects, sneic->InRedirects, itv),
			 out + 10, outsize + 10, svg_p->restart);
		/* oredir/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutRedirects, sneic->OutRedirects, itv),
			 out + 11, outsize + 11, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display TCPv4 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_tcp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr],
		*sntp = (struct stats_net_tcp *) a->buf[!curr];
	int group[] = {2, 2};
	char *title[] = {"TCPv4 network statistics (1)", "TCPv4 network statistics (2)"};
	char *g_title[] = {"active/s", "passive/s",
			   "iseg/s", "oseg/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(4, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 4, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* active/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sntp->ActiveOpens, sntc->ActiveOpens, itv),
			 out, outsize, svg_p->restart);
		/* passive/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sntp->PassiveOpens, sntc->PassiveOpens, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* iseg/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sntp->InSegs, sntc->InSegs, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* oseg/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sntp->OutSegs, sntc->OutSegs, itv),
			 out + 3, outsize + 3, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display TCPv4 network errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_etcp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr],
		*snetp = (struct stats_net_etcp *) a->buf[!curr];
	int group[] = {2, 3};
	char *title[] = {"TCPv4 network errors statistics (1)", "TCPv4 network errors statistics (2)"};
	char *g_title[] = {"atmptf/s", "estres/s",
			   "retrans/s", "isegerr/s", "orsts/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(5, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 5, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* atmptf/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snetp->AttemptFails, snetc->AttemptFails, itv),
			 out, outsize, svg_p->restart);
		/* estres/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snetp->EstabResets, snetc->EstabResets, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* retrans/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snetp->RetransSegs, snetc->RetransSegs, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* isegerr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snetp->InErrs, snetc->InErrs, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* orsts/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snetp->OutRsts, snetc->OutRsts, itv),
			 out + 4, outsize + 4, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display UDPv4 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_udp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr],
		*snup = (struct stats_net_udp *) a->buf[!curr];
	int group[] = {2, 2};
	char *title[] = {"UDPv4 network statistics (1)", "UDPv4 network statistics (2)"};
	char *g_title[] = {"idgm/s", "odgm/s",
			   "noport/s", "idgmerr/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(4, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 4, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* idgm/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->InDatagrams, snuc->InDatagrams, itv),
			 out, outsize, svg_p->restart);
		/* odgm/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->OutDatagrams, snuc->OutDatagrams, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* noport/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->NoPorts, snuc->NoPorts, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* idgmerr/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->InErrors, snuc->InErrors, itv),
			 out + 3, outsize + 3, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display IPV6 network socket statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_sock6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					  unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];
	int group[] = {4};
	char *title[] = {"IPv6 network sockets"};
	char *g_title[] = {"~tcp6sck", "~udp6sck", "~raw6sck", "~ip6-frag"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(4, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 0, 4, (void *) a->buf[curr], NULL,
			     itv, spmin, spmax);
		/* tcp6sck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->tcp6_inuse,
			  out, outsize, svg_p->restart);
		/* udp6sck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->udp6_inuse,
			  out + 1, outsize + 1, svg_p->restart);
		/* raw6sck */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->raw6_inuse,
			  out + 2, outsize + 2, svg_p->restart);
		/* ip6-frag */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) snsc->frag6_inuse,
			  out + 3, outsize + 3, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display IPv6 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_ip6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr],
		*snip = (struct stats_net_ip6 *) a->buf[!curr];
	int group[] = {4, 2, 2, 2};
	char *title[] = {"IPv6 network statistics (1)", "IPv6 network statistics (2)",
			 "IPv6 network statistics (3)", "IPv6 network statistics (4)"};
	char *g_title[] = {"irec6/s", "fwddgm6/s", "idel6/s", "orq6/s",
			   "asmrq6/s", "asmok6/s",
			   "imcpck6/s", "omcpck6/s",
			   "fragok6/s", "fragcr6/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(10, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(10, 0, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* irec6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InReceives6, snic->InReceives6, itv),
			 out, outsize, svg_p->restart);
		/* fwddgm6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutForwDatagrams6, snic->OutForwDatagrams6, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* idel6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InDelivers6, snic->InDelivers6, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* orq6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutRequests6, snic->OutRequests6, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* asmrq6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->ReasmReqds6, snic->ReasmReqds6, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* asmok6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->ReasmOKs6, snic->ReasmOKs6, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* imcpck6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InMcastPkts6, snic->InMcastPkts6, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* omcpck6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutMcastPkts6, snic->OutMcastPkts6, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* fragok6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->FragOKs6, snic->FragOKs6, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* fragcr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->FragCreates6, snic->FragCreates6, itv),
			 out + 9, outsize + 9, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display IPv6 network errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_eip6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr],
		*sneip = (struct stats_net_eip6 *) a->buf[!curr];
	int group[] = {4, 2, 2, 3};
	char *title[] = {"IPv6 network errors statistics (1)", "IPv6 network errors statistics (2)",
			 "IPv6 network errors statistics (3)", "IPv6 network errors statistics (4)",
			 "IPv6 network errors statistics (5)"};
	char *g_title[] = {"ihdrer6/s", "iadrer6/s", "iukwnp6/s", "i2big6/s",
			   "idisc6/s", "odisc6/s",
			   "inort6/s", "onort6/s",
			   "asmf6/s", "fragf6/s", "itrpck6/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(11, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(11, 0, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* ihdrer6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InHdrErrors6, sneic->InHdrErrors6, itv),
			 out, outsize, svg_p->restart);
		/* iadrer6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InAddrErrors6, sneic->InAddrErrors6, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* iukwnp6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InUnknownProtos6, sneic->InUnknownProtos6, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* i2big6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InTooBigErrors6, sneic->InTooBigErrors6, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* idisc6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InDiscards6, sneic->InDiscards6, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* odisc6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutDiscards6, sneic->OutDiscards6, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* inort6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InNoRoutes6, sneic->InNoRoutes6, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* onort6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutNoRoutes6, sneic->OutNoRoutes6, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* asmf6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->ReasmFails6, sneic->ReasmFails6, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* fragf6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->FragFails6, sneic->FragFails6, itv),
			 out + 9, outsize + 9, svg_p->restart);
		/* itrpck6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InTruncatedPkts6, sneic->InTruncatedPkts6, itv),
			 out + 10, outsize + 10, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display ICMPv6 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_icmp6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					  unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr],
		*snip = (struct stats_net_icmp6 *) a->buf[!curr];
	int group[] = {2, 3, 5, 3, 4};
	char *title[] = {"ICMPv6 network statistics (1)", "ICMPv6 network statistics (2)",
			 "ICMPv6 network statistics (3)", "ICMPv6 network statistics (4)",
			 "ICMPv6 network statistics (5)"};
	char *g_title[] = {"imsg6/s", "omsg6/s",
			   "iech6/s", "iechr6/s", "oechr6/s",
			   "igmbq6/s", "igmbr6/s", "ogmbr6/s", "igmbrd6/s", "ogmbrd6/s",
			   "irtsol6/s", "ortsol6/s", "irtad6/s",
			   "inbsol6/s", "onbsol6/s", "inbad6/s", "onbad6/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(17, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 17, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* imsg6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InMsgs6, snic->InMsgs6, itv),
			 out, outsize, svg_p->restart);
		/* omsg6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutMsgs6, snic->OutMsgs6, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* iech6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InEchos6, snic->InEchos6, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* iechr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InEchoReplies6, snic->InEchoReplies6, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* oechr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutEchoReplies6, snic->OutEchoReplies6, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* igmbq6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InGroupMembQueries6, snic->InGroupMembQueries6, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* igmbr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InGroupMembResponses6, snic->InGroupMembResponses6, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* ogmbr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutGroupMembResponses6, snic->OutGroupMembResponses6, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* igmbrd6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InGroupMembReductions6, snic->InGroupMembReductions6, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* ogmbrd6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutGroupMembReductions6, snic->OutGroupMembReductions6, itv),
			 out + 9, outsize + 9, svg_p->restart);
		/* irtsol6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InRouterSolicits6, snic->InRouterSolicits6, itv),
			 out + 10, outsize + 10, svg_p->restart);
		/* ortsol6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutRouterSolicits6, snic->OutRouterSolicits6, itv),
			 out + 11, outsize + 11, svg_p->restart);
		/* irtad6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InRouterAdvertisements6, snic->InRouterAdvertisements6, itv),
			 out + 12, outsize + 12, svg_p->restart);
		/* inbsol6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InNeighborSolicits6,snic->InNeighborSolicits6, itv),
			 out + 13, outsize + 13, svg_p->restart);
		/* onbsol6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutNeighborSolicits6, snic->OutNeighborSolicits6, itv),
			 out + 14, outsize + 14, svg_p->restart);
		/* inbad6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->InNeighborAdvertisements6, snic->InNeighborAdvertisements6, itv),
			 out + 15, outsize + 15, svg_p->restart);
		/* onbad6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snip->OutNeighborAdvertisements6, snic->OutNeighborAdvertisements6, itv),
			 out + 16, outsize + 16, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display ICMPv6 network errors statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_eicmp6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					   unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr],
		*sneip = (struct stats_net_eicmp6 *) a->buf[!curr];
	int group[] = {1, 2, 2, 2, 2, 2};
	char *title[] = {"ICMPv6 network errors statistics (1)", "ICMPv6 network errors statistics (2)",
			 "ICMPv6 network errors statistics (3)", "ICMPv6 network errors statistics (4)",
			 "ICMPv6 network errors statistics (5)", "ICMPv6 network errors statistics (6)"};
	char *g_title[] = {"ierr6/s",
			   "idtunr6/s", "odtunr6/s",
			   "itmex6/s", "otmex6/s",
			   "iprmpb6/s", "oprmpb6/s",
			   "iredir6/s", "oredir6/s",
			   "ipck2b6/s", "opck2b6/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(11, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 11, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* ierr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InErrors6, sneic->InErrors6, itv),
			 out, outsize, svg_p->restart);
		/* idtunr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InDestUnreachs6, sneic->InDestUnreachs6, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* odtunr6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutDestUnreachs6, sneic->OutDestUnreachs6, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* itmex6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InTimeExcds6, sneic->InTimeExcds6, itv),
			 out + 3, outsize + 3, svg_p->restart);
		/* otmex6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutTimeExcds6, sneic->OutTimeExcds6, itv),
			 out + 4, outsize + 4, svg_p->restart);
		/* iprmpb6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InParmProblems6, sneic->InParmProblems6, itv),
			 out + 5, outsize + 5, svg_p->restart);
		/* oprmpb6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutParmProblems6, sneic->OutParmProblems6, itv),
			 out + 6, outsize + 6, svg_p->restart);
		/* iredir6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InRedirects6, sneic->InRedirects6, itv),
			 out + 7, outsize + 7, svg_p->restart);
		/* oredir6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutRedirects6, sneic->OutRedirects6, itv),
			 out + 8, outsize + 8, svg_p->restart);
		/* ipck2b6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->InPktTooBigs6, sneic->InPktTooBigs6, itv),
			 out + 9, outsize + 9, svg_p->restart);
		/* opck2b6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(sneip->OutPktTooBigs6, sneic->OutPktTooBigs6, itv),
			 out + 10, outsize + 10, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display UDPv6 network statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_net_udp6_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr],
		*snup = (struct stats_net_udp6 *) a->buf[!curr];
	int group[] = {2, 2};
	char *title[] = {"UDPv6 network statistics (1)", "UDPv6 network statistics (2)"};
	char *g_title[] = {"idgm6/s", "odgm6/s",
			   "noport6/s", "idgmer6/s"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(4, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 4, 0, (void *) a->buf[curr], (void *) a->buf[!curr],
			     itv, spmin, spmax);

		/* idgm6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->InDatagrams6, snuc->InDatagrams6, itv),
			 out, outsize, svg_p->restart);
		/* odgm6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->OutDatagrams6, snuc->OutDatagrams6, itv),
			 out + 1, outsize + 1, svg_p->restart);
		/* noport6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->NoPorts6, snuc->NoPorts6, itv),
			 out + 2, outsize + 2, svg_p->restart);
		/* idgmer6/s */
		lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 S_VALUE(snup->InErrors6, snuc->InErrors6, itv),
			 out + 3, outsize + 3, svg_p->restart);
	}

	if (action & F_END) {
		draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH, title, g_title, NULL, group,
				     spmin, spmax, out, outsize, svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display CPU frequency statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pwr_cpufreq_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					    unsigned long long g_itv, struct record_header *record_hdr)
{
	struct stats_pwr_cpufreq *spc, *spp;
	int group[] = {1};
	char *title[] = {"CPU frequency"};
	char *g_title[] = {"MHz"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char item_name[8];
	int i;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* For each CPU */
		for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

			spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr]  + i * a->msize);
			spp = (struct stats_pwr_cpufreq *) ((char *) a->buf[!curr]  + i * a->msize);

			/* Should current CPU (including CPU "all") be displayed? */
			if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
				/* No */
				continue;

			/* MHz */
			recappend(record_hdr->ust_time - svg_p->ust_time_ref,
				  ((double) spp->cpufreq) / 100,
				  ((double) spc->cpufreq) / 100,
				  out + i, outsize + i, svg_p->restart, svg_p->dt,
				  spmin + i, spmax + i);
		}
	}

	if (action & F_END) {
		for (i = 0; (i < a->nr) && (i < a->bitmap->b_size + 1); i++) {

			/* Should current CPU (including CPU "all") be displayed? */
			if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
				/* No */
				continue;

			if (!i) {
				/* This is CPU "all" */
				strcpy(item_name, "all");
			}
			else {
				sprintf(item_name, "%d", i - 1);
			}

			draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH,
					     title, g_title, item_name, group,
					     spmin + i, spmax + i, out + i, outsize + i,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display fan statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pwr_fan_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					unsigned long long g_itv, struct record_header *record_hdr)
{
	struct stats_pwr_fan *spc, *spp;
	int group[] = {1};
	char *title[] = {"Fan speed"};
	char *g_title[] = {"~rpm"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char item_name[MAX_SENSORS_DEV_LEN + 8];
	int i;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* For each fan */
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_fan *) ((char *) a->buf[curr]  + i * a->msize);
			spp = (struct stats_pwr_fan *) ((char *) a->buf[!curr]  + i * a->msize);

			/* rpm */
			recappend(record_hdr->ust_time - svg_p->ust_time_ref,
				  (double) spp->rpm,
				  (double) spc->rpm,
				  out + i, outsize + i, svg_p->restart, svg_p->dt,
				  spmin + i, spmax + i);
		}
	}

	if (action & F_END) {
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_fan *) ((char *) a->buf[curr]  + i * a->msize);

			snprintf(item_name, MAX_SENSORS_DEV_LEN + 8, "%d: %s", i + 1, spc->device);
			item_name[MAX_SENSORS_DEV_LEN + 7] = '\0';

			draw_activity_graphs(a->g_nr, SVG_LINE_GRAPH,
					     title, g_title, item_name, group,
					     spmin + i, spmax + i, out + i, outsize + i,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display temperature statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pwr_temp_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
					 unsigned long long g_itv, struct record_header *record_hdr)
{
	struct stats_pwr_temp *spc;
	int group[] = {1};
	char *title1[] = {"Device temperature (1)"};
	char *title2[] = {"Device temperature (2)"};
	char *g1_title[] = {"~degC"};
	char *g2_title[] = {"%temp"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char item_name[MAX_SENSORS_DEV_LEN + 8];
	int i;
	double tval;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(2 * a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* For each temperature  sensor */
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_temp *) ((char *) a->buf[curr]  + i * a->msize);

			/* Look for min/max values */
			if (spc->temp < *(spmin + 2 * i)) {
				*(spmin + 2 * i) = spc->temp;
			}
			if (spc->temp > *(spmax + 2 * i)) {
				*(spmax + 2 * i) = spc->temp;
			}
			tval = (spc->temp_max - spc->temp_min) ?
			       (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			       0.0;
			if (tval < *(spmin + 2 * i + 1)) {
				*(spmin + 2 * i + 1) = tval;
			}
			if (tval > *(spmax + 2 * i + 1)) {
				*(spmax + 2 * i + 1) = tval;
			}

			/* degC */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 (double) spc->temp,
				 out + 2 * i, outsize + 2 * i, svg_p->restart);
			/* %temp */
			brappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 0.0, tval,
				 out + 2 * i + 1, outsize + 2 * i + 1, svg_p->dt);
		}
	}

	if (action & F_END) {
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_temp *) ((char *) a->buf[curr]  + i * a->msize);

			snprintf(item_name, MAX_SENSORS_DEV_LEN + 8, "%d: %s", i + 1, spc->device);
			item_name[MAX_SENSORS_DEV_LEN + 7] = '\0';

			draw_activity_graphs(1, SVG_LINE_GRAPH,
					     title1, g1_title, item_name, group,
					     spmin + 2 * i, spmax + 2 * i, out + 2 * i, outsize + 2 * i,
					     svg_p, record_hdr);
			draw_activity_graphs(1, SVG_BAR_GRAPH,
					     title2, g2_title, item_name, group,
					     spmin + 2 * i + 1, spmax + 2 * i + 1,
					     out + 2 * i + 1, outsize + 2 * i + 1,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display voltage inputs statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_pwr_in_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				       unsigned long long g_itv, struct record_header *record_hdr)
{
	struct stats_pwr_in *spc;
	int group[] = {1};
	char *title1[] = {"Voltage inputs (1)"};
	char *title2[] = {"Voltage inputs (2)"};
	char *g1_title[] = {"inV"};
	char *g2_title[] = {"%in"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	char item_name[MAX_SENSORS_DEV_LEN + 8];
	int i;
	double tval;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(2 * a->nr, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* For each temperature  sensor */
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_in *) ((char *) a->buf[curr]  + i * a->msize);

			/* Look for min/max values */
			if (spc->in < *(spmin + 2 * i)) {
				*(spmin + 2 * i) = spc->in;
			}
			if (spc->in > *(spmax + 2 * i)) {
				*(spmax + 2 * i) = spc->in;
			}
			tval = (spc->in_max - spc->in_min) ?
			       (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			       0.0;
			if (tval < *(spmin + 2 * i + 1)) {
				*(spmin + 2 * i + 1) = tval;
			}
			if (tval > *(spmax + 2 * i + 1)) {
				*(spmax + 2 * i + 1) = tval;
			}

			/* inV */
			lnappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 (double) spc->in,
				 out + 2 * i, outsize + 2 * i, svg_p->restart);
			/* %in */
			brappend(record_hdr->ust_time - svg_p->ust_time_ref,
				 0.0, tval,
				 out + 2 * i + 1, outsize + 2 * i + 1, svg_p->dt);
		}
	}

	if (action & F_END) {
		for (i = 0; i < a->nr; i++) {

			spc = (struct stats_pwr_in *) ((char *) a->buf[curr]  + i * a->msize);

			snprintf(item_name, MAX_SENSORS_DEV_LEN + 8, "%d: %s", i + 1, spc->device);
			item_name[MAX_SENSORS_DEV_LEN + 7] = '\0';

			draw_activity_graphs(1, SVG_LINE_GRAPH,
					     title1, g1_title, item_name, group,
					     spmin + 2 * i, spmax + 2 * i, out + 2 * i, outsize + 2 * i,
					     svg_p, record_hdr);
			draw_activity_graphs(1, SVG_BAR_GRAPH,
					     title2, g2_title, item_name, group,
					     spmin + 2 * i + 1, spmax + 2 * i + 1,
					     out + 2 * i + 1, outsize + 2 * i + 1,
					     svg_p, record_hdr);
		}

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}

/*
 ***************************************************************************
 * Display huge pages statistics in SVG.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 * @action	Action expected from current function.
 * @svg_p	SVG specific parameters: Current graph number (.@graph_no),
 * 		flag indicating that a restart record has been previously
 * 		found (.@restart) and time used for the X axis origin
 * 		(@ust_time_ref).
 * @itv		Interval of time in jiffies (only with F_MAIN action).
 * @record_hdr	Pointer on record header of current stats sample.
 ***************************************************************************
 */
__print_funct_t svg_print_huge_stats(struct activity *a, int curr, int action, struct svg_parm *svg_p,
				     unsigned long long itv, struct record_header *record_hdr)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];
	int group1[] = {2};
	int group2[] = {1};
	char *title1[] = {"Huge pages utilization (1)"};
	char *title2[] = {"Huge pages utilization (2)"};
	char *g1_title[] = {"~kbhugfree", "~kbhugused"};
	char *g2_title[] = {"%hugused"};
	static double *spmin, *spmax;
	static char **out;
	static int *outsize;
	double tval;

	if (action & F_BEGIN) {
		/*
		 * Allocate arrays that will contain the graphs data
		 * and the min/max values.
		 */
		out = allocate_graph_lines(3, &outsize, &spmin, &spmax);
	}

	if (action & F_MAIN) {
		/* Check for min/max values */
		save_extrema(0, 1, 0, (void *) a->buf[curr], NULL,
			     itv, spmin, spmax);

		if (smc->tlhkb - smc->frhkb < *(spmin + 1)) {
			*(spmin + 1) = smc->tlhkb - smc->frhkb;
		}
		if (smc->tlhkb - smc->frhkb > *(spmax + 1)) {
			*(spmax + 1) = smc->tlhkb - smc->frhkb;
		}
		tval = smc->tlhkb ? SP_VALUE(smc->frhkb, smc->tlhkb, smc->tlhkb) : 0.0;
		if (tval < *(spmin + 2)) {
			*(spmin + 2) = tval;
		}
		if (tval > *(spmax + 2)) {
			*(spmax + 2) = tval;
		}

		/* kbhugfree */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) smc->frhkb,
			  out, outsize, svg_p->restart);
		/* hugused */
		lniappend(record_hdr->ust_time - svg_p->ust_time_ref,
			  (unsigned long) smc->tlhkb - smc->frhkb,
			  out + 1, outsize + 1, svg_p->restart);
		/* %hugused */
		brappend(record_hdr->ust_time - svg_p->ust_time_ref,
			 0.0, tval,
			 out + 2, outsize + 2, svg_p->dt);
	}

	if (action & F_END) {
		draw_activity_graphs(1, SVG_LINE_GRAPH,
				     title1, g1_title, NULL, group1,
				     spmin, spmax, out, outsize, svg_p, record_hdr);
		draw_activity_graphs(1, SVG_BAR_GRAPH,
				     title2, g2_title, NULL, group2,
				     spmin + 2, spmax + 2, out + 2, outsize + 2,
				     svg_p, record_hdr);

		/* Free remaining structures */
		free_graphs(out, outsize, spmin, spmax);
	}
}
