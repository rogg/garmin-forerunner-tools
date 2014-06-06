/*
  Garmintools software package
  Copyright (C) 2006-2008 Dave Bailey

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "garmin.h"


#define BBOX_NW  0
#define BBOX_NE  1
#define BBOX_SE  2
#define BBOX_SW  3


typedef struct
{
  float lat, lon, elev;
  time_t t;
  int lap;
  int pause;
  uint8 hr;
  uint8 cad;
} route_point;

int
get_gpx_data ( garmin_data *    fulldata,
               route_point   *** tracks,
               position_type *  sw,
               position_type *  ne )
{
  garmin_list *       dlist;
  garmin_list_node *  node;
  garmin_list_node *  lapnode;
  garmin_data *       point;
  D304 *              d304;
  route_point *       rp;
  float               minlat =   90.0;
  float               maxlat =  -90.0;
  float               minlon =  180.0;
  float               maxlon = -180.0;
  int                 ok     = 0;
  garmin_data *		data;
  garmin_list *     glaps;
  garmin_data*     points;

  if ( fulldata != NULL ) {

    // lists: 1=laps, 2=points

    glaps=garmin_list_data(fulldata,1)->data; // get lap information

    // fixme: crash if we didn't get it
    int laps=glaps->elements+1;

    *tracks = malloc(sizeof(route_point) * (laps));
    memset(*tracks,0,laps*sizeof(route_point));

    int curlapnum=0;

    lapnode=glaps->head;
    D1015* lapdata=lapnode->data->data;

    data = garmin_list_data(fulldata, 2); // get track points

    if ( data == NULL ) {

      printf("get_gpx_data: no track points found\n");

    } else if ( data->type == data_Dlist ) {

      dlist = data->data;

      points = malloc(sizeof(route_point) * (dlist->elements+1+(laps*2)));
      (*tracks)[curlapnum]=points;
      rp = points;

      int pause=0;

      for ( node = dlist->head; node != NULL; node = node->next ) {
	point = node->data;

	switch (point->type) {
	  case data_D304: // position point

	    d304 = point->data;

	    if ( d304->posn.lat == 0x7fffffff && d304->posn.lon == 0x7fffffff ) {
	      pause++;
	      continue;
	    }

	    rp->lap=0;
	    if (lapdata!=NULL) {
	      if (d304->time >= lapdata->start_time) {
		if (curlapnum>0) {
		  rp->t=0; // end previous lap
		  rp++;
		  (*tracks)[curlapnum]=rp; // new track
		}
		curlapnum++;

		rp->lap=curlapnum;
		if (d304->time != lapdata->start_time) {
		  // if lap start point doesn't exist, create it
		  rp->lat = SEMI2DEG(lapdata->begin.lat);
		  rp->lon = SEMI2DEG(lapdata->begin.lon);

		  rp->elev = d304->alt; // lap data doesn't contain alt :(
		  rp->hr=d304->heart_rate;
		  rp->cad=d304->cadence;

		  rp->t = lapdata->start_time;
		  rp++;
		  rp->lap=0;
		  rp->pause=0;
		}
		lapnode=lapnode->next;
		if (lapnode!=NULL)
		  lapdata=lapnode->data->data;
		else
		  lapdata=NULL; // last lap
	      }
	    }

	    rp->lat = SEMI2DEG(d304->posn.lat);
	    rp->lon = SEMI2DEG(d304->posn.lon);
	    rp->elev = d304->alt;
	    rp->t = d304->time;
	    rp->hr=d304->heart_rate;
	    rp->cad=d304->cadence;


	    if (pause==2) {
	      rp->pause=1;
	      pause=0;
	    } else rp->pause=0;

	    if ( rp->lat < minlat ) minlat = rp->lat;
	    if ( rp->lat > maxlat ) maxlat = rp->lat;
	    if ( rp->lon < minlon ) minlon = rp->lon;
	    if ( rp->lon > maxlon ) maxlon = rp->lon;

	    ++rp;
	    break;
	  default: //printf("unknown frame %i\n", point->type);
	    ;
	}
      }
      rp->t = 0;

      /* Now we can fill in the center coordinate and bounding box. */

      ne->lat = DEG2SEMI(maxlat);
      ne->lon = DEG2SEMI(maxlon);

      sw->lat = DEG2SEMI(minlat);
      sw->lon = DEG2SEMI(minlon);

      ok = 1;
    } else {
      printf("get_gpx_data: invalid track point list\n");
    }
  } else {
    printf("get_gpx_data: NULL data pointer\n");
  }

  return ok;
}


static void
print_spaces ( FILE * fp, int spaces )
{
  int i;

  for ( i = 0; i < spaces; i++ ) {
    fprintf(fp," ");
  }
}


static void
print_open_tag ( const char * tag, FILE * fp, int spaces )
{
  print_spaces(fp,spaces);
  fprintf(fp,"<%s>\n",tag);
}


static void
print_close_tag ( const char * tag, FILE * fp, int spaces )
{
  print_spaces(fp,spaces);
  fprintf(fp,"</%s>\n",tag);
}


static void
print_string_tag ( const char * tag, const char * val, FILE * fp, int spaces )
{
  print_spaces(fp,spaces);
  fprintf(fp,"<%s>%s</%s>\n",tag,val,tag);
}


static void
print_gpx_header ( FILE *               fp,
                   int                  spaces )
{
  print_spaces(fp,spaces);
  fprintf(fp,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp,"<gpx version=\"1.1\"\n"
    "creator=\"Garmin Forerunner Tools - http://garmintools.googlecode.com\"\n"
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    "xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\"\n"
    "xmlns=\"http://www.topografix.com/GPX/1/1\"\n"
    "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 "
    "http://www.topografix.com/GPX/1/1/gpx.xsd "
    "http://www.garmin.com/xmlschemas/TrackPointExtension/v1 "
    "http://www8.garmin.com/xmlschemas/TrackPointExtensionv1.xsd\">\n");
}

static void
print_time_tag ( const time_t           t,
                 FILE *                 fp,
                 int                    spaces )
{
  char buf[512];
  struct tm *tmp;

  tmp = gmtime(&t);
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tmp);
  print_string_tag("time",buf,fp,spaces);
}

static void
print_bounds_tag ( const position_type * sw,
                   const position_type * ne,
                   FILE *                fp,
                   int                   spaces )
{
  print_spaces(fp,spaces);
  fprintf(fp,"<bounds minlat=\"%f\" minlon=\"%f\" maxlat=\"%f\" maxlon=\"%f\" />\n",
      SEMI2DEG(sw->lat), SEMI2DEG(sw->lon),
      SEMI2DEG(ne->lat), SEMI2DEG(ne->lon));
}

static void
print_route_points ( route_point * points,
                     FILE *        fp,
                     int           spaces )
{
  route_point * rp = points;
  while (rp->t > 0) {
    print_spaces(fp, spaces);
    fprintf(fp, "<trkpt lat=\"%f\" lon=\"%f\">\n", rp->lat, rp->lon);
    print_spaces(fp, spaces+2);
    fprintf(fp, "<ele>%f</ele>\n", rp->elev);
    print_time_tag(rp->t + TIME_OFFSET, fp, spaces+2);
    if (rp->lap) {
      print_spaces(fp, spaces+2);
      fprintf(fp,"<name>Lap %i</name>\n",rp->lap);
    } else if (rp->pause) {
      print_spaces(fp, spaces+2);
      fprintf(fp,"<name>Pause</name>\n");
    }

    if ((rp->hr != 0) || (rp->cad != 0xff)) {
      print_spaces(fp, spaces+2);
      fprintf(fp,"<extensions>\n");
      print_spaces(fp, spaces+2);
      fprintf(fp,"<gpxtpx:TrackPointExtension>\n");
      if (rp->hr != 0) {
	print_spaces(fp, spaces+4);
	fprintf(fp,"<gpxtpx:hr>%i</gpxtpx:hr>\n",rp->hr);
      }
      if (rp->cad != 0xff) {
	print_spaces(fp, spaces+4);
	fprintf(fp,"<gpxtpx:cad>%i</gpxtpx:cad>\n",rp->cad);
      }

      print_close_tag("gpxtpx:TrackPointExtension", fp, spaces+2);
      print_close_tag("extensions", fp, spaces+2);
    }

    print_close_tag("trkpt", fp, spaces);
    ++rp;
  }
}

void
print_gpx_data ( garmin_data * data, FILE * fp, int spaces )
{
  route_point **         laps = NULL;
  route_point * points;
  position_type  sw;
  position_type  ne;

  if ( get_gpx_data(data,&laps,&sw,&ne) != 0 ) {
    print_gpx_header(fp,spaces);
    print_time_tag(time(NULL),fp,spaces);
    print_bounds_tag(&sw,&ne,fp,spaces);

    int i;
    print_open_tag("trk",fp,spaces);
    for (i=0; laps[i]!=NULL; i++) {
      points=laps[i];
      print_open_tag("trkseg",fp,spaces);
      print_route_points(points,fp,spaces+2);
      print_close_tag("trkseg",fp,spaces);
    }
    print_close_tag("trk",fp,spaces);
    print_close_tag("gpx",fp,spaces);

    free(laps[i]);
    free(laps);
  }
}


int
main ( int argc, char ** argv )
{
  garmin_data * data;
  int           i;
  
  for ( i = 1; i < argc; i++ ) {
    if ( (data = garmin_load(argv[i])) != NULL ) {
      print_gpx_data(data,stdout,0);
      garmin_free_data(data);
    }
  }
  
  return 0;
}
