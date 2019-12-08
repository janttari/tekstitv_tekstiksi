#include <stdio.h>
#include <stdlib.h>

#define TXT_ROWS			24
#define TXT_COLUMNS			40
#define BLACK		0
#define BLUE		1
#define GREEN		2
#define CYAN 	  	3
#define RED	   		4
#define MAGENTA		5
#define ORANGE 		6
#define WHITE		7
#define GREY		8
#define BLUE_I		9
#define GREEN_I		10
#define CYAN_I		11
#define ORANGE_I	12/*COULD BE RED*/
#define RED_I		12/*COULD BE ORANGE*/
#define MAGENTA_I	13
#define YELLOW_I	14
#define YELLOW		14
#define WHITE_I		15

#define PAGLEN            969     /*length of data used from teletext page*/

int main(int argc, char** argv)
{
int a, c, x, y;
int graphicsflag; /* not if graphics hold */
int gsepaflag;  
int bgcolor = BLACK;
int fgcolor = WHITE;
int escapeflag; 
int flashflag;  
int dheightflag;
int concealflag;
int gholdflag;  
int boxflag;
int soflag;
int dheightline;
char temp[1024];
int ptr, ptrb;
char dheight_xpositions_background[TXT_COLUMNS];
int last_graphic_character;
int last_background, last_foreground, foreground, background;

/* display the page as text */

/* the following flags and colors are reset at the start of every page */
/* none */
for(y = 0; y < TXT_ROWS; y++)/*every line in txt page*/
	{
	/*the following flags are reset at the start of each line*/
	graphicsflag = 0; /* not if graphics hold */
	gsepaflag = 0;
	bgcolor = BLACK;
	fgcolor = WHITE;
	escapeflag = 0;
	flashflag = 0;
	dheightflag = 0;
	concealflag = 0;
	dheightflag = 0;
	gholdflag = 0;
	boxflag = 0;
	soflag = 0;
	dheightline = 0;
	ptr = y * TXT_COLUMNS;
	fprintf(stdout, "\r\n");/* new line */
	for(x = 0; x < TXT_COLUMNS; x++)/*every column in txt page*/
		{
		ptrb = ptr + x;
		if(ptrb > PAGLEN)break;
 		a = getchar() & 0x7f;

		if(a > 31)		/*not a control character*/
			{
			/* 0x20 trouhg 0x3f first 32 graphics characters teletext */
			/* 0x60 trough 0x7f second 32 */
			/* s14font graphics 0xa0 through 0xff (is 160 trough 255)
			/* first half s14font graphics at 0xa0,
			/* second half s14font graphics at 0xe0.
			*/
			if(graphicsflag)	/*display as graphics if valid, else text */
				{
				if( (a >= 0x20) && (a <= 0x3f) )	/* upper group */
					{
					a += (0xa0 - 0x20);/* upper half of graphics set s14font*/
					}
				if( (a >= 0x60) && (a <= 0x7f) ) /* lower group */
					{
					a += (0xe0 - 0x60);/* lower half of graphics set s14font */
					}
				} /* end graphics flag */
			/* print code here */
			temp[0] = a;
			temp[1] = 0;/* string termination */

			if(dheightflag)
				{
				/* remember where and what bgcolor */
				dheight_xpositions_background[x] = -1;
				if(a >= 127)fprintf(stdout, " ");
				else fprintf(stdout, "%c", temp[0]);
				}
			else
				{
				dheight_xpositions_background[x] = bgcolor;
				if(a >= 127)fprintf(stdout, " ");
				else fprintf(stdout, "%c", temp[0]);
				}
			if(graphicsflag)
				{
				last_graphic_character = temp[0];/* save for graphics hold */
				}/* IF ADDED */
				
			last_background = bgcolor;
			last_foreground = fgcolor;
			}/* end not a control character */
		else		/* < 32 is control characters*/
			{
			concealflag = 0;/* reset conceal flag*/
			/*control characters appear as spaces*/
			switch(a)				/*control character sort*/
				{
				case 0:
					fgcolor = BLACK;
					graphicsflag = 0;
					break;
				case 1:
					fgcolor = RED;	/* using intensified, 
										luminance of red to low to read*/
					graphicsflag = 0;
					break;
				case 2:
					fgcolor = GREEN;
					graphicsflag = 0;
					break;
				case 3:
					fgcolor = YELLOW;
					graphicsflag = 0;
					break;
				case 4:
					fgcolor = BLUE;
					graphicsflag = 0;
					break;
				case 5:
					fgcolor = MAGENTA;
					graphicsflag = 0;
					break;
				case 6:
					fgcolor = CYAN;
					graphicsflag = 0;
					break;
				case 7:
					fgcolor = WHITE;
					graphicsflag = 0;
					break;
				case 8:				/*flash*/
					flashflag = 1;
					break;
				case 9:				/*steady*/
					flashflag = 0;
					break;
				case 10:			/*end box*/
					boxflag = 0;
					break;
				case 11:			/*start box*/
					boxflag = 1;
					break;
				case 12:			/*normal height*/
					dheightflag = 0;
					break;
				case 13:			/*double height*/
					dheightflag = 1;
					dheightline = 1;/* flag if set causes next line to be 
									erased at x pos. where no dheight in
									this line */
				break;
				case 14:			/*SO unknown command*/
					soflag = 1;
					break;
				case 15:			/*SI unknown command*/
					soflag = 0;
					break;
				case 16:
					fgcolor = BLACK;
					graphicsflag = 1;
					break;
				case 17:
					fgcolor = RED;	/* using intensified,
										luminance od red to low to read */
					graphicsflag = 1;
					break;
				case 18:
					fgcolor = GREEN;
					graphicsflag = 1;
					break;
				case 19:
					fgcolor = YELLOW;
					graphicsflag = 1;
					break;
				case 20:
					fgcolor = BLUE;
					graphicsflag = 1;
					break;
				case 21:
					fgcolor = MAGENTA;
					graphicsflag = 1;
					break;
				case 22:
					fgcolor = CYAN;
					graphicsflag = 1;
					break;
				case 23:
					fgcolor = WHITE;
					graphicsflag = 1;
					break;
				case 24:
				 	concealflag = 1;
					break;
				case 25:			/*continuous graphics*/
					gsepaflag = 0;
					break;
				case 26:			/*separated graphics*/
					gsepaflag = 1;
					graphicsflag = 1;
					break;
				case 27:			/*escape*/
					escapeflag = 1;
					break;
				case 28:			/*black background*/
					bgcolor = BLACK;
					break;
				case 29:			/*new background*/
					gholdflag = 0;
					bgcolor = fgcolor;
					break;
				case 30:			/*hold graphics*/
					gholdflag = 1;
/*					graphicsflag = 1;*/
					break;
				case 31:			/*release graphics*/
					gholdflag = 0;
					break;
				}
			if(gholdflag)/* repeat last graphics char */
				{
				foreground = last_foreground;
				background = last_background;
				temp[0] = ' ';/*last_graphic_character;*/
				}
			else
				{
				foreground = fgcolor;
				background = bgcolor;
				temp[0] = ' ';/* space */
				}
			temp[1] = 0;/* string termination */
			if(dheightflag)
				{
				dheight_xpositions_background[x] = -1;/* remember where */
				fprintf(stdout, " ");
				}
			else
				{
				dheight_xpositions_background[x] = background;
				fprintf(stdout, " ");
				}
			}/*end control characters*/
		}/*end for each column*/
	}/*end for each line*/

fprintf(stdout, "\r\n");

return(1);
} /* end function main */



