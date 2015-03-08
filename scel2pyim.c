#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char unicode2utf8char(unsigned short in, unsigned char *out)
{
  if (in >= 0x0000 && in <= 0x007f)
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
  else if (in >= 0x0800 && in <= 0xffff)
    {
      out[0] = 0xe0 | (in >> 12);
      out[1] = 0x80 | ((in >> 6) & 0x3f);
      out[2] = 0x80 | (in & 0x3f);
      return 3;
    }
  else
    return -1;
}

char unicode2utf8str(unsigned short in[], int insize, unsigned char out[])
{
  unsigned char str[3+1];
  int i;
  *out = 0;
  for (i = 0; i < insize; i++)
    {
      str[unicode2utf8char(in[i], str)] = 0;
      strcat(out, str);
      if (in[i] == 0) break;
    }
  return 0;
}

void mkpydict(FILE *in, char *pydict[])
{
  int i;
  unsigned short addr, size, code[6];
  char *p;
  fseek(in, 0x1544, SEEK_SET);
  for (i = 0 ; i < 413; i++)
    {
      p = (char*)malloc(7);
      fread(&addr, 2, 1, in);
      fread(&size, 2, 1, in);
      fread(code, 2, size/2, in);
      unicode2utf8str(code, size/2, p);
      pydict[addr] = p;
    }
}

void mkpyimpylist(char *pydict[], unsigned short pysize, unsigned short pylistcode[], char pylist[])
{
  unsigned short i, j = pysize - 1;
  for (i = 0; i < pysize; i++)
    {
      strcat(pylist, pydict[pylistcode[i]]);
      if (j)
	{
	  strcat(pylist, "-");
	  j--;
	}
    }
}

void mkpyimwdlist(char *pydict[], unsigned short same, unsigned short wdsize[], unsigned short *wdlistcode[], char wdlist[])
{
  unsigned short i, j = same - 1;
  char *p;
  for (i = 0; i < same; i++)
    {
      p = (char*)malloc(3*wdsize[i]+1);
      unicode2utf8str(wdlistcode[i], wdsize[i]/2, p);
      strcat(wdlist, p);
      free(p);
      if (j)
	{
	  strcat(wdlist, " ");
	  j--;
	}
    }
}

void writepyim(FILE *in, FILE *out, char *pydict[])
{
  unsigned short same, pysize, *pylistcode, *wdsize, **wdlistcode, extrsize, *extrlist, *p, i;
  char *pylist, *wdlist;
  unsigned int len = 0;
  long end;
  fseek(in, 0, SEEK_END);
  end = ftell(in);
  fseek(in, 0x2628, SEEK_SET);
  while (ftell(in) != end)
    {
      fread(&same, 2, 1, in);
      fread(&pysize, 2, 1, in);
      pylistcode = (unsigned short*)malloc(pysize);
      fread(pylistcode, 2, pysize/2, in);
      pylist = (char*)malloc(pysize*7);
      pylist[0] = 0;
      mkpyimpylist(pydict, pysize/2, pylistcode, pylist);
      fprintf(out, "%s ", pylist);
      free(pylistcode);
      free(pylist);
      wdsize = (unsigned short*)malloc(same*2);
      wdlistcode = (unsigned short**)malloc(same*2);
      for (i = 0; i < same; i++)
	{
	  fread(&wdsize[i], 2, 1, in);
	  wdlistcode[i] = (unsigned short*)malloc(wdsize[i]);
	  fread(wdlistcode[i], 2, wdsize[i]/2, in);
	  fread(&extrsize, 2, 1, in);
	  extrlist = (unsigned short*)malloc(extrsize);
	  fread(extrlist, 2, extrsize/2, in);
	  free(extrlist);
	}
      for (i = 0; i < same; i++)
	len += wdsize[i];
      len /= 2;
      wdlist = (char*)malloc(3*len+1);
      wdlist[0] = 0;
      mkpyimwdlist(pydict, same, wdsize, wdlistcode, wdlist);
      fprintf(out, "%s\n", wdlist);
      free(wdlist);
      free(*wdlistcode);
      free(wdsize);
    }
}

char readandwrite(FILE *in, FILE *out)
{
  char buffer[5], *pydict[413], n;
  unsigned short code;
  fseek(in, 0x1540, SEEK_SET);
  fgets(buffer,4, in);
  if(memcmp(buffer,"\x9D\x01\x00\x00",4) != 0)
    {
      printf("Can not find the beginning.\n");
      return -1;
    }
  mkpydict(in, pydict);
  writepyim(in, out, pydict);
}

char information(FILE *in)
{
  char i;
  unsigned short code[32];
  unsigned char outstr[128];
  
  fseek(in, 0x130, SEEK_SET);
  fread(code, 2, 32, in);
  unicode2utf8str(code, 32, outstr);
  printf("字库名称:%s\n", outstr);
  
  fseek(in, 0x338, SEEK_SET);
  fread(code, 2, 32, in);
  unicode2utf8str(code, 32, outstr);
  printf("字库类别:%s\n",outstr);
  
  fseek(in, 0x540, SEEK_SET);
  fread(code, 2, 32, in);
  unicode2utf8str(code, 32, outstr);
  printf("字库信息:%s\n", outstr);
  
  fseek(in, 0xd40, SEEK_SET);
  for (i = 0; i < 32; i++)
    {
      fread(&code[i], 2, 1, in);
      if (code[i] == 0x000d)
	i--;
      else if (code[i] == 0x0020)
	{
	  code[i] = ' ';
	  unicode2utf8str(code, 32, outstr);
	}
    }  
  printf("字库示例:%s\n", outstr); 
}

int main(int argc, char *argv[])
{
  char str[9];
  FILE *in, *out;
  if (argc != 3)
    {
      printf("Usage : %s name.sbcl file_out.\n", argv[0]);
      return 1;
    }
  in = fopen(argv[1], "rb");
  if (in == NULL)
    {
      fclose(in);
      printf("Could not find \"%s\"\n", argv[1]);
      return 1;
    }
  fgets(str, 8+1, in);
  if(memcmp(str, "\x40\x15\x00\x00\x44\x43\x53\x01", 8))
    {
      fclose(in);
      printf("\"%s\" is really not a .scel file.\n", argv[1]);
      return 1;
    }
  else
    {
      out = fopen(argv[2], "wb");
      information(in);
      printf("converting...\n");
      fprintf(out, ";; -*- coding: utf-8\n");
      readandwrite(in, out);
      printf("Done.\n");
    }
  fclose(in);
  fclose(out);
  return 0;
}
