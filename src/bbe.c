/*
 *    bbe - Binary block editor
 *
 *    Copyright (C) 2005 Timo Savinen
 *    This file is part of bbe.
 * 
 *    bbe is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    bbe is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with bbe; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* $Id: bbe.c,v 1.43 2006-03-12 10:05:33 timo Exp $ */

#include "bbe.h"

#ifdef HAVE_GETOPT_H

#include <getopt.h>

#endif

#include <ctype.h>
#include <stdlib.h>

#ifdef WIN32

#include <share.h>

#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef PACKAGE
static char *program = PACKAGE;
#else
static char *program = "bbe";
#endif

#ifdef VERSION
static char *version = VERSION;
#else
static char *version = "0.1.7";
#endif

#ifdef PACKAGE_BUGREPORT
static char *email_address = PACKAGE_BUGREPORT;
#else
static char *email_address = "tjsa@iki.fi";
#endif

int const MAX_TOKEN = 10;


/**
 * global block
 */
struct block block;

/**
 * commands to be executed
 */
struct commands cmds;

/**
 * extra info for panic
 */
char *panic_info = NULL;

/**
 * -s switch state
 */
int output_only_block = 0;

/**
 * c command conversions
 */
char *convert_strings[] = {
    "BCDASC",
    "ASCBCD",
    "",
};
/**
 * commands to be executed at start of buffer
 */
#define BLOCK_START_COMMANDS "KDIJLFBN>"

/**
 * commands to be executed for each byte
 */
#define BYTE_COMMANDS "acdirstywjpl&|^~ufx"

/**
 * commands to be executed at end of buffer
 */
#define BLOCK_END_COMMANDS "A<"

/**
 * format types for p command
 */
char *p_formats = "DOHAB";

/**
 * formats for F and B commands
 */
char *FB_formats = "DOH";

static char short_opts[] = "b:g:e:f:o:s?V";

#ifdef HAVE_GETOPT_LONG
static struct option long_opts[] = {
    {"block",1,NULL,'b'},
    {"block-file",1,NULL,'g'},
    {"expression",1,NULL,'e'},
    {"file",1,NULL,'f'},
    {"output",1,NULL,'o'},
    {"help",0,NULL,'?'},
    {"version",0,NULL,'V'},
    {"suppress",0,NULL,'s'},
    {NULL,0,NULL,0}
};
#endif

/**
 * Stop the program in a consistent way.
 */
void
panic(char *msg, char *info, char *syserror) {
  if (panic_info != NULL) fprintf(stderr, "%s: %s", program, panic_info);

  if (info == NULL)
    if (syserror == NULL)
      fprintf(stderr, "%s: %s\n", program, msg);
    else
      fprintf(stderr, "%s: %s: %s\n", program, msg, syserror);
  else if (syserror == NULL)
    fprintf(stderr, "%s: %s: %s\n", program, msg, info);
  else
    fprintf(stderr, "%s: %s: %s: %s\n", program, msg, info, syserror);

  exit(EXIT_FAILURE);
}

/**
 * Stop the program in a consistent way and report the command causing the error.
 */
void
panic_c(char *msg, char action, char *info, char *syserror) {
  if (panic_info != NULL) fprintf(stderr, "%s: %s", program, panic_info);

  if (action == '\0')
    if (info == NULL)
      if (syserror == NULL)
        fprintf(stderr, "%s: %s\n", program, msg);
      else
        fprintf(stderr, "%s: %s: %s\n", program, msg, syserror);
    else if (syserror == NULL)
      fprintf(stderr, "%s: %s: %s\n", program, msg, info);
    else
      fprintf(stderr, "%s: %s: %s: %s\n", program, msg, info, syserror);
  else if (info == NULL)
    if (syserror == NULL)
      fprintf(stderr, "%s: %s: %c\n", program, msg, action);
    else
      fprintf(stderr, "%s: %s: %c: %s\n", program, msg, action, syserror);
  else if (syserror == NULL)
    fprintf(stderr, "%s: %s: %c: %s\n", program, msg, action, info);
  else
    fprintf(stderr, "%s: %s: %c: %s: %s\n", program, msg, action, info, syserror);

  exit(EXIT_FAILURE);
}

/**
 * parse a long int, can start with n (dec), x (hex), 0 (oct)
 */
off_t
parse_long(char *long_int) {
  long long int l;
  char *scan = long_int;
  char type = 'd';           // others are x and o


  if (*scan == '0') {
    type = 'o';
    scan++;
    if (*scan == 'x' || *scan == 'X') {
      type = 'x';
      scan++;
    }
  }

  while (*scan != 0) {
    switch (type) {
      case 'd':
        if (!isdigit(*scan)) panic("Error in number", long_int, NULL);
        break;
      case 'o':
        if (!isdigit(*scan) || *scan >= '8') panic("Error in number", long_int, NULL);
        break;
      case 'x':
        if (!isxdigit(*scan)) panic("Error in number", long_int, NULL);
        break;
    }
    scan++;
  }

  if (sscanf(long_int, "%lli", &l) != 1) {
    panic("Error in number", long_int, NULL);
  }
  return (off_t) l;
}

/**
 * parse a string, string can contain \n, \xn, \0n and \\ escape codes.
 * memory will be allocated
 */
struct pattern
parse_string(char *string, struct pattern *target) {
  char *p;
  int j, k, i = 0;
  int min_len;
  unsigned char buf[INPUT_BUFFER_LOW + 1];
  char num[5];
  unsigned char *ret;

  p = string;

  while (*p != 0) {
    if (*p == '\\') {
      p++;
      if (strchr("\\;abtnvfr", *p) != NULL) {
        switch (*p) {
          case 'a':
            buf[i] = '\a';
            break;
          case 'b':
            buf[i] = '\b';
            break;
          case 't':
            buf[i] = '\t';
            break;
          case 'n':
            buf[i] = '\n';
            break;
          case 'v':
            buf[i] = '\v';
            break;
          case 'f':
            buf[i] = '\f';
            break;
          case 'r':
            buf[i] = '\r';
            break;
          default:
            buf[i] = *p;
        }
        p++;
      } else {
        j = 0;
        switch (*p) {
          case 'x':
          case 'X':
            num[j++] = '0';
            num[j++] = *p++;
            while (isxdigit(*p) && j < 4) num[j++] = *p++;
            min_len = 3;
            break;
          case '0':
            while (isdigit(*p) && *p < '8' && j < 4) num[j++] = *p++;
            min_len = 1;
            break;
          default:
            while (isdigit(*p) && j < 3) num[j++] = *p++;
            min_len = 1;
            break;
        }
        num[j] = 0;
        if (sscanf(num, "%i", &k) != 1 || j < min_len) {
          panic("Syntax error in escape code", string, NULL);
        }
        if (k < 0 || k > 255) {
          panic("Escape code not in range (0-255)", string, NULL);
        }
        buf[i] = (unsigned char) k;
      }
    } else {
      buf[i] = (unsigned char) *p++;
    }
    if (i > INPUT_BUFFER_LOW) {
      panic("string too long", string, NULL);
    }
    i++;
  }
  if (i > 0) {
    target->string = (unsigned char *) xmalloc(i);
    memcpy(target->string, buf, i);
  } else {
    target->string = NULL;
  }
  target->length = i;
  return *target;
}


/**
 * parse a block definition and save it to block
 */
static void
parse_block(char *bs, int length) {
  char slash_char;
  char *p = bs;
  int i = 0;
  char *buf;
  char *after = bs + length;

  if (length > (2 * 4 * INPUT_BUFFER_LOW)) {
    panic("Block definition too long", NULL, NULL);
  }

  buf = xmalloc(2 * 4 * INPUT_BUFFER_LOW);
  block.type = 0;
  // note: the block start and stop are a union so the initial values are irrelevant.

  if (*p == ':') {
    // no start block is provided.
    // the start block defaults to immediate.
    block.type |= BLOCK_START_S;
    block.start.S.length = 0;
  } else {
    if (*p == 'x' || *p == 'X' || isdigit(*p)) {
      block.type |= BLOCK_START_M;
      switch (*p) {
        case 'x':
        case 'X':
          buf[i++] = '0';
          buf[i++] = *p++;
          while (isxdigit(*p)) buf[i++] = *p++;
          break;
        case '0':
          while (isdigit(*p) && *p < '8') buf[i++] = *p++;
          break;
        default:
          while (isdigit(*p)) buf[i++] = *p++;
          break;
      }

      buf[i] = 0;
      block.start.N = parse_long(buf);
    } else                                // string start
    {
      block.type |= BLOCK_START_S;
      slash_char = *p;
      p++;
      while (*p != slash_char && *p != 0) buf[i++] = *p++;
      if (*p == slash_char) p++;
      buf[i] = 0;
      parse_string(buf, &block.start.S);
    }
  }

  if (*p != ':') {
    panic("Error in block definition", bs, NULL);
  }

  p++;

  if (p < after) {
    i = 0;
    if (*p == 'x' || *p == 'X' || isxdigit(*p)) {
      block.type |= BLOCK_STOP_M;
      switch (*p) {
        case 'x':
        case 'X':
          buf[i++] = '0';
          buf[i++] = *p++;
          while (isxdigit(*p)) buf[i++] = *p++;
          break;
        case '0':
          while (isdigit(*p) && *p < '8') buf[i++] = *p++;
          break;
        default:
          while (isdigit(*p)) buf[i++] = *p++;
          break;
      }
      buf[i] = 0;
      block.stop.M = parse_long(buf);
      if (block.stop.M == 0) panic("Block length must be greater than zero", NULL, NULL);
    } else {
      block.type |= BLOCK_STOP_S;
      if (*p == '$') {
        block.stop.S.length = 0;
        p++;
      } else {
        slash_char = *p;
        p++;
        while (*p != slash_char && *p != 0) buf[i++] = *p++;
        if (*p == slash_char) {
          p++;
        } else {
          panic("syntax error in block definition", bs, NULL);
        }
        buf[i] = 0;
        parse_string(buf, &block.stop.S);
      }
    }
  } else {
    block.type |= BLOCK_STOP_S;
    block.stop.S.length = 0;
  }
  if (p != after) {
    panic("syntax error in block definition", bs, NULL);
  }
  free(buf);
}

/**
 * parse one command, commands are in list pointed by commands
 */
void
parse_command(char *command_string) {
  struct command_list *curr, *new, **start;
  char *c, *p, *buf;
  char *f;
  char *token[MAX_TOKEN];
  char slash_char;
  int i, j;

  p = command_string;
  while (isspace(*p)) p++;              // remove leading spaces
  if (p[0] == 0) return;      // empty line
  if (p[0] == '#') return;       // comment

  c = xstrdup(p);

  i = 0;
  token[i] = strtok(c, " \t\n");
  i++;
  while (token[i - 1] != NULL && i < MAX_TOKEN) token[i++] = strtok(NULL, " \t\n");
  i--;

  if (strchr(BLOCK_START_COMMANDS, token[0][0]) != NULL) {
    curr = cmds.block_start;
    start = &cmds.block_start;
  } else if (strchr(BYTE_COMMANDS, token[0][0]) != NULL) {
    curr = cmds.byte;
    start = &cmds.byte;
  } else if (strchr(BLOCK_END_COMMANDS, token[0][0]) != NULL) {
    curr = cmds.block_end;
    start = &cmds.block_end;
  } else {
    panic_c("Error unknown command", token[0][0], command_string, NULL);
  }

  if (curr != NULL) {
    while (curr->next != NULL) curr = curr->next;
  }
  new = xmalloc(sizeof(struct command_list));
  new->next = NULL;
  if (curr == NULL) {
    *start = new;
  } else {
    curr->next = new;
  }


  new->letter = token[0][0];
  switch (new->letter) {
    case 'D':
    case 'K':
      if (i < 1 || i > 2 || strlen(token[0]) > 1)
        panic_c("Error in command ", new->letter, command_string, NULL);
      if (i == 2) {
        new->offset = parse_long(token[1]);
        if (new->offset < 1) panic("n for D-command must be at least 1", NULL, NULL);
      } else {
        new->offset = 0;
      }
      break;
    case 'A':
    case 'I':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command ", new->letter, command_string, NULL);
      parse_string(token[1], &new->s1);
      break;
    case 'w':
    case '<':
    case '>':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->s1.string = xstrdup(token[1]);
      break;
    case 'j':
    case 'J':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->count = parse_long(token[1]);
      break;
    case 'l':
    case 'L':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->count = parse_long(token[1]);
      break;
    case 'r':
    case 'i':
      if (i != 3 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->offset = parse_long(token[1]);
      parse_string(token[2], &new->s1);
      break;
    case 'd':
      if (i < 2 || i > 3 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->offset = parse_long(token[1]);

      if (token[2][0] == '*' && !token[2][1]) {
        new->count = 0;
      } else {
        new->count = parse_long(token[2]);
        if (new->count < 1) panic_c("Error in command", new->letter, command_string, NULL);
      }
      break;
    case 'c':
      if (i != 3 || strlen(token[1]) != 3 || strlen(token[2]) != 3 || strlen(token[0]) > 1)
        panic_c("Error in command", new->letter, command_string, NULL);
      new->s1.string = xmalloc(strlen(token[1]) + strlen(token[2]) + 2);
      strcpy(new->s1.string, token[1]);
      strcat(new->s1.string, token[2]);
      j = 0;
      while (new->s1.string[j] != 0) {
        new->s1.string[j] = toupper(new->s1.string[j]);
        j++;
      }
      j = 0;
      while (*convert_strings[j] != 0 && strcmp(convert_strings[j], new->s1.string) != 0) j++;
      if (*convert_strings[j] == 0) panic_c("Unknown conversion", new->letter, command_string, NULL);
      break;
    case 's':
    case 't':
    case 'y':
      if (strlen(command_string) < 4) panic_c("Error in command", new->letter, command_string, NULL);

      buf = xmalloc((4 * INPUT_BUFFER_LOW) + 1);

      slash_char = command_string[1];
      p = command_string;
      p += 2;
      j = 0;
      while (*p != 0 && *p != slash_char && j < 4 * INPUT_BUFFER_LOW) buf[j++] = *p++;
      if (*p != slash_char) panic_c("Error in command", new->letter, command_string, NULL);
      buf[j] = 0;
      parse_string(buf, &new->s1);
      if (new->s1.length > INPUT_BUFFER_LOW) panic("String in command too long", command_string, NULL);
      if (new->s1.length == 0) panic_c("Error in command", new->letter, command_string, NULL);

      p++;

      j = 0;
      while (*p != 0 && *p != slash_char && j < 4 * INPUT_BUFFER_LOW) buf[j++] = *p++;
      buf[j] = 0;
      if (*p != slash_char) panic_c("Error in command", new->letter, command_string, NULL);
      parse_string(buf, &new->s2);
      if (new->s2.length > INPUT_BUFFER_LOW) panic_c("String in command too long", new->letter, command_string, NULL);

      if (new->letter == 'y' && new->s1.length != new->s2.length)
        panic("Strings in y-command must have equal length", command_string, NULL);
      free(buf);
      break;
    case 'F':
    case 'B':
      if (i > 1 && (strlen(token[1]) != 1)) panic_c("Error in command", new->letter, command_string, NULL);
    case 'p':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      parse_string(token[1], &new->s1);
      j = 0;
      while (new->s1.string[j] != 0) {
        new->s1.string[j] = toupper(new->s1.string[j]);
        j++;
      }
      if (new->letter == 'p') {
        f = p_formats;
      } else {
        f = FB_formats;
      }
      while (*f != 0 && strchr(new->s1.string, *f) == NULL) f++;
      if (*f == 0) panic_c("Error in command", new->letter, command_string, NULL);
      break;
    case 'N':
      if (i != 1 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      break;
    case '&':
    case '|':
    case '^':
      if (i != 2 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      parse_string(token[1], &new->s1);
      if (new->s1.length != 1) panic_c("Error in command", new->letter, command_string, NULL);
      break;
    case '~':
    case 'x':
      if (i != 1 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      break;
    case 'u':
    case 'f':
      if (i != 3 || strlen(token[0]) > 1) panic_c("Error in command", new->letter, command_string, NULL);
      new->offset = parse_long(token[1]);
      parse_string(token[2], &new->s1);
      if (new->s1.length != 1) panic_c("Error in command", new->letter, command_string, NULL);
      break;
    default:
      panic_c("Unknown command", new->letter, command_string, NULL);
      break;
  }
  free(c);
}

/**
 * parse commands, commands are separated by ';'.
 * ';' can be escaped as '\;'.
 * ';'s inside " or ' are not separators;
 */
void
parse_commands(char *command_string) {
  char *c;
  char *start;
  int inside_d = 0;  // double
  int inside_s = 0;  // single

  c = command_string;
  start = c;

  while (*start != 0) {
    switch (*c) {
      case '\\':
        c++;
        break;
      case '"':
        if (inside_d) {
          inside_d--;
        } else {
          inside_d++;
        }
        break;
      case '\'':
        if (inside_s) {
          inside_s--;
        } else {
          inside_s++;
        }
        break;
      case ';':
        if (!inside_d && !inside_s) {
          *c = 0;
          parse_command(start);
          start = c + 1;
        }
        break;
      case 0:
        parse_command(start);
        start = c;
        break;
    }
    c++;
  }
}


/**
 * parse commands in a file.
 * commands are in list read commands from file
 */
void
parse_command_file(char *file) {
  FILE *fp;
  char *line;
  char *info;
  size_t line_len = (8 * 1024);
  int line_no = 0;

  line = xmalloc(line_len);
  info = xmalloc(strlen(file) + 100);

#ifdef WIN32
  errno_t rc = fopen_s(&fp, file, "rb");
  if (rc != 0) panic("Error opening command file", file, strerror(rc));
#else
  fp = fopen(file,"r");
  if (fp == NULL) panic("Error opening command file",file,strerror(errno));
#endif

#ifdef HAVE_GETLINE
  while(getline(&line,&line_len,fp) != -1)
#else
  while (fgets(line, line_len, fp) != NULL)
#endif
  {
    line_no++;
    sprintf(info, "Error in file '%s' in line %d\n", file, line_no);
    panic_info = info;
    parse_commands(line);
  }

  free(line);
  free(info);
  fclose(fp);
  panic_info = NULL;
}

/**
 * parse one block definition, only one block is in the file.
 * The block definition is the entire file.
 */
void
parse_block_file(char *file) {
#ifdef WIN32
  FILE *fp;
  errno_t rc = fopen_s(&fp, file, "rb");
  if (rc != 0) panic("Error opening block description file", file, strerror(rc));
#else
  FILE * fp = fopen(file,"r");
  if (fp == NULL) panic("Error opening block description file",file,strerror(errno));
#endif

  fseek(fp, 0, SEEK_END);
  long length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buffer = xmalloc(length);
  if (!buffer) {
    fclose(fp);
    char *info = xmalloc(strlen(file) + 100);
    sprintf(info, "Error in file '%s'\n", file);
    panic_info = info;
    panic_info = NULL;
    return;
  }
  fread(buffer, 1, length, fp);
  fclose(fp);
  parse_block(buffer, length);
  free(buffer);
  panic_info = NULL;
}

void
help(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTION]...\n\n", program);
#ifdef HAVE_GETOPT_LONG
  fprintf(stream,"-b, --block=BLOCK\n");
  fprintf(stream,"\t\tBlock definition.\n");
  fprintf(stream,"-g, --block-file=block-file\n");
  fprintf(stream,"\t\tAdd block definition from block-file.\n");
  fprintf(stream,"-e, --expression=COMMAND\n");
  fprintf(stream,"\t\tAdd command to the commands to be executed.\n");
  fprintf(stream,"-f, --file=script-file\n");
  fprintf(stream,"\t\tAdd commands from script-file to the commands to be executed.\n");
  fprintf(stream,"-o, --output=name\n");
  fprintf(stream,"\t\tWrite output to name instead of standard output.\n");
  fprintf(stream,"-s, --suppress\n");
  fprintf(stream,"\t\tSuppress normal output, print only block contents.\n");
  fprintf(stream,"-?, --help\n");
  fprintf(stream,"\t\tDisplay this help and exit.\n");
  fprintf(stream,"-V, --version\n");
#else
  fprintf(stream, "-b BLOCK\n");
  fprintf(stream, "\t\tBlock definition.\n");
  fprintf(stream, "-g block-file\n");
  fprintf(stream, "\t\tAdd a block definition from block-file.\n");
  fprintf(stream, "-e COMMAND\n");
  fprintf(stream, "\t\tAdd command to the commands to be executed.\n");
  fprintf(stream, "-f script-file\n");
  fprintf(stream, "\t\tAdd commands from script-file to the commands to be executed.\n");
  fprintf(stream, "-o name\n");
  fprintf(stream, "\t\tWrite output to name instead of standard output.\n");
  fprintf(stream, "-s\n");
  fprintf(stream, "\t\tSuppress normal output, print only block contents.\n");
  fprintf(stream, "-?\n");
  fprintf(stream, "\t\tDisplay this help and exit.\n");
  fprintf(stream, "-V\n");
#endif
  fprintf(stream, "\t\tShow version and exit.\n");
  fprintf(stream, "\nAll remaining arguments are names of input files;\n");
  fprintf(stream, "if no input files are specified, then the standard input is read.\n");
  fprintf(stream, "\nSend bug reports to %s.\n", email_address);
}

void
usage(int opt) {
  printf("Unknown option '-%c'\n", (char) opt);
  help(stderr);
}

void
print_version() {
  printf("%s version %s\n", program, version);
  printf("Copyright (c) 2005 Timo Savinen\n\n");
  printf("This is free software; see the source for copying conditions.\n");
  printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}


int
main(int argc, char **argv) {
  int opt;

  block.type = 0;
  cmds.block_start = NULL;
  cmds.byte = NULL;
  cmds.block_end = NULL;
#ifdef HAVE_GETOPT_LONG
  while ((opt = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1)
#else
  while ((opt = getopt(argc, argv, short_opts)) != -1)
#endif
  {
    switch (opt) {
      case 'b':
        if (block.type) panic("Only one -b option allowed", NULL, NULL);
        parse_block(optarg, strlen(optarg));
        break;
      case 'g':
        parse_block_file(optarg);
        break;
      case 'e':
        parse_commands(optarg);
        break;
      case 'f':
        parse_command_file(optarg);
        break;
      case 'o':
        set_output_file(optarg);
        break;
      case 's':
        output_only_block = 1;
        break;
      case '?':
        help(stdout);
        exit(EXIT_SUCCESS);
        break;
      case 'V':
        print_version();
        exit(EXIT_SUCCESS);
        break;
      default:
        usage(opt);
        exit(EXIT_FAILURE);
        break;
    }
  }
  if (!block.type) parse_block("0:$", 3);
  if (out_stream.file == NULL) set_output_file(NULL);

  if (optind < argc) {
    while (optind < argc) set_input_file(argv[optind++]);
  } else {
    set_input_file("-");
  }

  init_buffer();
  init_commands(&cmds);
  execute_program(&cmds);
  close_commands(&cmds);
  exit(EXIT_SUCCESS);
}
