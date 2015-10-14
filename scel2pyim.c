/* Copyright (C) 2015 E-Neo <e-neo@qq.com>

   This file is part of scel2pyim.

   scel2pyim is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   scel2pyim is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with scel2pyim.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
unicode2utf8char (unsigned short in, char *out)
{
  if (in <= 0x007f)
    {
      out[0] = in;
      return 1;
    }
  else if (in >= 0x0080 && in <= 0x07ff)
    {
      out[0] = 0xc0 | (in >> 6);
      out[1] = 0x80 | (in & 0x3f);
      return 2;
    }
  else if (in >= 0x0800)
    {
      out[0] = 0xe0 | (in >> 12);
      out[1] = 0x80 | ((in >> 6) & 0x3f);
      out[2] = 0x80 | (in & 0x3f);
      return 3;
    }
  else
    return -1;
}

int
unicode2utf8str (unsigned short *in, int insize, char *out)
{
  char str[3+1];
  int i;
  *out = 0;
  for (i = 0; i < insize; i++)
    {
      if (in[i] > 0 && in[i] < 20) continue;
      str[unicode2utf8char(in[i], str)] = 0;
      strcat (out, str);
      if (in[i] == 0) break;
    }
  return 0;
}

void
mkpydict (FILE *in, char *pydict[]) /* Create a pinyin dictionary. */
{
  int i;
  unsigned short addr, size, code[6];
  char *p;
  fseek (in, 0x1544, SEEK_SET);
  for (i = 0 ; i < 413; i++)
    {
      p = (char*)malloc(7);
      fread (&addr, 2, 1, in);
      fread (&size, 2, 1, in);
      fread (code, 2, size/2, in);
      unicode2utf8str (code, size/2, p);
      pydict[addr] = p;
    }
}

void
mkpyimpylist (char *pydict[], unsigned short pysize,
              unsigned short pylistcode[], char pylist[])
{
  unsigned short i, j = pysize - 1;
  for (i = 0; i < pysize; i++)
    {
      strcat (pylist, pydict[pylistcode[i]]);
      if (j)
	{
	  strcat (pylist, "-");
	  j--;
	}
    }
}

void
writepyim (FILE *in, FILE *out, char *pydict[])
{
  unsigned short same, pysize, *pylistcode;
  unsigned short *wdsize, *wdlistcode, extrsize, *extrlist, i, j;
  char *pylist, *wdlist;
  long end;
  fseek (in, 0, SEEK_END);
  end = ftell(in);
  fseek (in, 0x2628, SEEK_SET);
  while (ftell(in) != end)
    {
      fread (&same, 2, 1, in);
      fread (&pysize, 2, 1, in);
      pylistcode = (unsigned short*) malloc(pysize);
      fread (pylistcode, 2, pysize/2, in);
      pylist = (char*)malloc (pysize*7);
      pylist[0] = 0;
      mkpyimpylist (pydict, pysize/2, pylistcode, pylist);
      fprintf (out, "%s ", pylist);
      free (pylistcode);
      free (pylist);
      wdsize = (unsigned short*)malloc (same*2);
      for (i = 0, j = same - 1; i < same; i++, j--)
	{
	  fread (&wdsize[i], 2, 1, in);
	  wdlistcode = (unsigned short*)malloc (wdsize[i]);
	  fread (wdlistcode, 2, wdsize[i]/2, in);
	  wdlist = (char*)malloc (3*wdsize[i]+1);
	  unicode2utf8str (wdlistcode, wdsize[i]/2, wdlist);
	  free (wdlistcode);
	  fprintf (out, "%s", wdlist);
	  if (j) fprintf(out, " ");
	  free (wdlist);
	  fread (&extrsize, 2, 1, in);
	  extrlist = (unsigned short*)malloc (extrsize);
	  fread (extrlist, 2, extrsize/2, in);
	  free (extrlist);
	}
      fprintf (out, "\n");
      //printf("%lx\n", ftell(in)); /* Only for debugging purpose. */
    }
}

char
readandwrite (FILE *in, FILE *out)
{
  char buffer[5], *pydict[413];
  fseek (in, 0x1540, SEEK_SET);
  fread (buffer,1, 4, in);
  if(memcmp (buffer, "\x9D\x01\x00\x00", 4) != 0)
    {
      printf ("Can not find the beginning.\n");
      return -1;
    }
  mkpydict (in, pydict);
  writepyim (in, out, pydict);
  return 0;
}

char
information (FILE *in)
{
  unsigned short code[0x1000];
  char outstr[0x2000+1];
  /* Decode Name. */
  fseek (in, 0x130, SEEK_SET);
  fread (code, 2, 0x208, in);
  unicode2utf8str (code, 0x208, outstr);
  printf ("Name:        %s\n", outstr);
  /* Decode Type. */
  fseek (in, 0x338, SEEK_SET);
  fread (code, 2, 0x208, in);
  unicode2utf8str (code, 0x208, outstr);
  printf ("Type:        %s\n",outstr);
  /* Decode Information. */
  fseek (in, 0x540, SEEK_SET);
  fread (code, 2, 0x800, in);
  unicode2utf8str (code, 0x800, outstr);
  printf ("Information: %s\n", outstr);
  /* Decode Examples. */
  fseek (in, 0xd40, SEEK_SET);
  fread (code, 2, 0x800, in);
  unicode2utf8str (code, 0x800, outstr);
  printf ("Examples:    %s\n", outstr);
  return 0;
}

int main(int argc, char *argv[])
{
  char str[12];
  FILE *in, *out;
  if (argc != 3)
    {
      fprintf(stderr, "Usage : %s /path/to/NAME.scel /path/to/NAME.pyim\n", argv[0]);
      return 1;
    }
  if ((in=fopen(argv[1], "rb")) == NULL)
    {
      fprintf(stderr, "Could not find \"%s\"\n", argv[1]);
      return 1;
    }
  fread(str, 1, 12, in);
  if(memcmp(str, "\x40\x15\x00\x00\x44\x43\x53\x01\x01\x00\x00\x00", 12))
    {
      fclose(in);
      fprintf(stderr, "\"%s\" is really not a .scel file.\n", argv[1]);
      return 1;
    }
  out = fopen(argv[2], "wb");
  //information(in);
  //printf("converting...\n");
  fprintf(out, ";; -*- coding: utf-8-unix; -*-\n");
  readandwrite(in, out);
  //printf("Done.\n");
  fclose(in);
  fclose(out);
  return 0;
}
