/* 
Copyright Jan Panteltje 2004 (panteltje@yahoo.com).

jpvtx, a teletext decoder that can be used to transparently pipe a
full transponder transport stream through it.
It reads the teletext pid[s] it uses from ~/jpcam_pids, or from the command line.
This is intended be used together with jpcam (decoder), and xvtx-p (viewer).


Based on dvbtext by (C) Dave Chapman <dave@dchapman.com> 2001.
dvbtext - a teletext decoder for DVB cards. 
(C) Dave Chapman <dave@dchapman.com> 2001.

The latest version can be found at http://www.linuxstb.org/dvbtext

Thanks to:  

Ralph Metzler for his work on both the DVB driver and his old
vbidecode package (some code and ideas in dvbtext are borrowed
from vbidecode).

Copyright notice:

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "tables.h"


#define VERSION "0.2.1"
#define VTXDIR "/tmp/vtx"
#define USAGE "\nUSAGE: jpvtx [tpid1 tpid2 tpid3 .. tpid8]\n\
if no arguments pids are read from ~/jpcam_pids\n\n"

// There seems to be a limit of 8 teletext streams - OK for most (but
// not all) transponders.
#define MAX_CHANNELS 9

int debug_flag;


typedef struct mag_struct_ {
   int valid;
   int mag;
   unsigned char flags;
   unsigned char lang;
   int pnum,sub;
   unsigned char pagebuf[25*40];
} mag_struct;


struct passwd *userinfo;
char *home_dir;
char *user_name;


char *strsave(char *s) /*save char array s somewhere*/
{
//char *p, *malloc();
char *p;
p = (char*) malloc(100);

p = malloc( strlen(s) +  1);
if(!p) return 0;

strcpy(p, s);

return(p);
} /* end function strsave */


// FROM vbidecode
// unham 2 bytes into 1, report 2 bit errors but ignore them
unsigned char unham(unsigned char a, unsigned char b)
{
unsigned char c1, c2;
  
c1 = unhamtab[a];
c2 = unhamtab[b];
//if( (c1 | c2) & 0x40) fprintf(stderr, "jpvtx: bad ham!");

return (c2 << 4) | (0x0f & c1);
} /* end function unham */


void write_data(unsigned char* b, int n)
{
int i;

for(i = 0; i < n; i++)
	{
	fprintf(stderr, " %02x", b[i]);
	}

fprintf(stderr, "\n");

for(i = 0; i < n; i++)
	{
	fprintf(stderr, "%c", b[i] & 0x7f);
	}

fprintf(stderr, "\n");
} /* end function write_data */


void set_line(mag_struct *mag, int line, unsigned char* data, int pnr)
{
unsigned char c;
FILE * fd;
char fname[80];
char muunnakomento[120]="./muunna-ascii ";
unsigned char buf;

if(line==0)
	{
	mag->valid = 1;
	memset(mag->pagebuf, ' ', 25 * 40);
	mag->pnum = unham(data[0], data[1]); // The lower two (hex) numbers of page
	if(mag->pnum == 0xff) return;  // These are filler lines. Can use to update clock

//	fprintf(stderr, "jpvtx: pagenum: %03x\n", c + mag->mag * 0x100);

	mag->flags = unham(data[2], data[3]) & 0x80;
	mag->flags |= (c & 0x40) | ((c >> 2) & 0x20);
	c = unham(data[6], data[7]);
	mag->flags |= ((c << 4) & 0x10)|((c << 2) & 0x08) | (c & 0x04) | ((c >> 1) & 0x02) | ((c >> 4) & 0x01);
	mag->lang = ((c >> 5) & 0x07);

	mag->sub = (unham(data[4], data[5]) << 8) | (unham(data[2], data[3]) & 0x3f7f);

//	mag->pnum = (mag->mag << 8) | ((mag->sub & 0x3f7f) << 16) | page;
//	fprintf(stderr, "jpvtx: page: %x, pnum: %x, sub: %x\n", page, mag->pnum, mag->sub);
	}

if(mag->valid)
	{
	if(line <= 23)
		{
		memcpy(mag->pagebuf + 40 * line, data, 40);
		}

	if(line == 23)
		{
		sprintf(fname, "%s/%d/%03x_%02x.vtx", VTXDIR, pnr, mag->pnum + (mag->mag * 0x100), mag->sub & 0xff);
//		fprintf(stderr, "jpvtx: writing to file %s\n", fname);
		if((fd = fopen(fname, "w")))
			{
			fwrite("VTXV4", 1, 5, fd);
			buf = 0x01;			fwrite(&buf, 1, 1, fd);
			buf = mag->mag;		fwrite(&buf, 1, 1, fd);
			buf = mag->pnum;	fwrite(&buf, 1, 1, fd);
			buf = 0x00;			fwrite(&buf, 1, 1, fd);    /* subpage?? */
			buf = 0x00;			fwrite(&buf, 1, 1, fd);  
			buf = 0x00;			fwrite(&buf, 1, 1, fd);  
			buf = 0x00;			fwrite(&buf, 1, 1, fd);  
			fwrite(mag->pagebuf, 1, 24 * 40, fd);
			fclose(fd);
            strcat(muunnakomento, fname);
            int suorita=system(muunnakomento);
            
			}
		mag->valid = 0;
		}
	}
} /* end function set_line */


int main(int argc, char **argv)
{
int pid;
int a, b, c, i, j, m;
unsigned char buf[188]; /* data buffer */
unsigned char mpag, mag, pack;
FILE *fptr;
char filename[4096];
char temp[80];

/* Test channels - Astra 19E, 11837h, SR 27500 - e.g. ARD */
int pids[MAX_CHANNELS];
int pnrs[MAX_CHANNELS] = { 1, 2, 3, 4, 5, 6, 7, 8 }; /* Directory names */

mag_struct mags[MAX_CHANNELS][8];
int count;

fprintf(stderr, "jpvtx-%s (c) Jan Panteltje 2005 \n", VERSION);
/* get user info */ 
userinfo = getpwuid(getuid() );

/* get home directory */
home_dir = strsave(userinfo -> pw_dir);
if(! home_dir)
    {
    fprintf(stderr, "jpvtx: main(): could allocte space for home_dir\n");
    exit(1);
    }

user_name = strsave(userinfo -> pw_name);
if(! user_name) 
    {
    fprintf(stderr, "jpvtx: main(): could allocate space for user_name\n");
    exit(1);
    }

if(argc > 1) // some command line arguments
	{ 
	count=0;
	for(i = 1; i < argc; i++)
		{
		sscanf(argv[i], "%i", &pids[count]);
	
		if (pids[count])
			{
			count++ ;
			}
		}
	
	if(count)
		{
		if(count > 8)
			{
			fprintf(stderr, "\njpvtx: sorry, you can only set up to 8 filters.\n\n");
			return(-1);
			}
		else
			{
			fprintf(stderr,\
			"jpvtx: decoding %d teletext stream%s specified on the command line into %s/*\n",\
			count, (count==1 ? "" : "s"), VTXDIR);
			}
		}
	else
		{
		fprintf(stderr, USAGE);
		return(-1);
		}

	for (m = 0; m < count; m++)
		{
   		mags[m][0].mag = 8;
   		for(i = 1; i < 8; i++)
			{ 
			mags[m][i].mag = i; 
			mags[m][i].valid = 0;
			}
		}

	if(debug_flag)
		{
		fprintf(stderr, "jpvtx: command line argc=%d count=%d\n", argc, count);
		}
	} /* end if command line arguments */

sprintf(filename, "%s/jpcam_pids", home_dir);
while (1)
	{
	a = fread(buf, 1, 188 ,stdin);
	if(a != 188)
		{
		fprintf(stderr, "jpvtx: could read only %d bytes of 188 from stdin, aborting.\n", a);

		exit(1);
		}

	a = fwrite(buf, 1, 188, stdout);
	if(a != 188)
		{
		fprintf(stderr, "jpvtx: could only write %d bytes of 188 to stdout, aborting.\n", a);

		exit(1);
		}

	/*
	It may take a while before jpcam writes the ~/jpcam_pid, so wait for that file.
	In this version we wait for ~/jpcam_pids to be created.
	The idea is that you call all this from a script that erases the old ~/jpcam_pids, like this:

	rm ~/jpcam_pids
	rm /video/vtx/1/???_??.vtx  
	dvbstream -f 11131 -p v -s 5632 -o 8192 | jpvtx | jpcam -p 5001 | xine -D -

	*/	

	if( (argc == 1) && (count == 0) ) // no arguments and ~/jpcam_pids not yet open.
		{
		a = 0;
		fptr = fopen(filename, "r");
		if(fptr)
			{
			/* file may be open for write by jpcam,
			but not yet have any data (teletext pids) written to it,
			so we try open - read - close again and again until we have at least one teletext pid.
			*/
			i = 0;
			while(1)
				{
				b = fscanf(fptr, "%s %d", temp, &c);
				if(b == EOF) break;

				if( (b == 2) && (strcmp(temp, "teletext_pid") == 0) )
					{
					pids[i] = c;
					a = 1;
					fprintf(stderr, "jpvtx: found teletext_pid %d in %s.\n", pids[i], filename);

					i++;
					if(i > 8) break;
					}
				}
			fclose(fptr);
			count = i;
			} /* end if reading pids from ~jpcam_pids */

		if(count == 0)
			{
			if(debug_flag) fprintf(stderr, "jpvtx: no teletext pids found in %s.\n", filename); 
			}
		else
			{
			fprintf(stderr, "jpvtx: %d teletext pids found in %s.\n", count, filename);

			for (m = 0; m < count; m++)
				{
   				mags[m][0].mag = 8;
   				for(i = 1; i < 8; i++)
					{ 
					mags[m][i].mag = i; 
					mags[m][i].valid = 0;
					}
				}
			}
		} /* end if no command line arguments and no teletext pids found yet in ~jpcam_pids */

	if(count == 0) continue;

	m = -1;
	pid = ((buf[1] & 0x1f) << 8) | buf[2];
	
	for(i = 0; i < count; i++)
		{
		if(pid == pids[i])
			{ 
//			fprintf(stderr, "jpvtx: received packet for PID %i\n", pid);
			m = i; 
			}
		}

	if( (pid != pids[0]) && (pid != pids[1]) && (pid != pids[2]) && (pid != pids[3]) && (pid != pids[4]) &&\
	(pid != pids[5]) && (pid != pids[6]) && (pid != pids[7]) )
		{
		continue;
		}

	if (pid != -1)
		{
		for(i = 0; i < 4; i++)
			{
			if(buf[4 + i * 46] == 2)
				{
				for(j = (8 + i * 46); j < (50 + i * 46); j++)
					{
					buf[j] = invtab[buf[j]];
					}

				mpag = unham(buf[0x8 + i * 46], buf[0x9 + i * 46]);
				mag = mpag & 7;
				pack = (mpag >> 3) & 0x1f;
				set_line(&mags[m][mag], pack, &buf[10 + i * 46], pnrs[m]);
//				fprintf(stderr, "jpvtx: i: %d mag=%d, pack=%d\n", i, mag, pack);
				}
			}

		} /* end if pid != -1 */

	} /* end while */

exit(0);
} /* end function main */


