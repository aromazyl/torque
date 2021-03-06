/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

/*
 *
 * xpbs_scriptload - parses a job script file, separating PBS directive
 *  from non-PBS lines. Job attributes are reported to stdout,
 *  and non-PBS lines are shown in script_tmp. This code
 *  is a modification of "qsub".
 *
 * Authors:
 *      Terry Heidelberg
 *      Livermore Computing
 *
 *      Bruce Kelly
 *      National Energy Research Supercomputer Center
 *
 *      Lawrence Livermore National Laboratory
 *      University of California
 *
 * Albeaus Bayucan
 *      Sterling Software
 *      NASA Ames Research Center
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <termios.h>
#include <string>
#ifdef sun
#include <sys/stream.h>
#endif /* sun */

#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#if !defined(sgi) && !defined(_AIX) && !defined(linux)
#include <sys/tty.h>
#endif  /* ! sgi */

#include "cmds.h"


#define MAX_QSUB_PREFIX_LEN 32

#ifndef PBS_DEPEND_LEN
#define PBS_DEPEND_LEN 2040
#endif
static char PBS_DPREFIX_DEFAULT[] = "#PBS";

/* globals */

int inter_sock;

struct termios oldtio;

struct winsize wsz;

struct attrl *attrib = NULL;
char *new_jobname;                  /* return from submit request */
char dir_prefix[MAX_QSUB_PREFIX_LEN+1];
char path_out[MAXPATHLEN+1];
char destination[PBS_MAXDEST];

/* state booleans for protecting already-set options */
int a_opt = FALSE;
int c_opt = FALSE;
int e_opt = FALSE;
int h_opt = FALSE;
int j_opt = FALSE;
int k_opt = FALSE;
int l_opt = FALSE;
int m_opt = FALSE;
int o_opt = FALSE;
int p_opt = FALSE;
int q_opt = FALSE;
int r_opt = FALSE;
int u_opt = FALSE;
int v_opt = FALSE;
int z_opt = FALSE;
int A_opt = FALSE;
int C_opt = FALSE;
int M_opt = FALSE;
int N_opt = FALSE;
int S_opt = FALSE;
int V_opt = FALSE;
int Depend_opt    = FALSE;
int Interact_opt  = FALSE;
int Stagein_opt   = FALSE;
int Stageout_opt  = FALSE;
int Grouplist_opt = FALSE;
char *v_value = NULL;

char *
set_dir_prefix(char *prefix, int diropt)
  {
  char *s;

  if (notNULL(prefix))
    return(prefix);
  else if (diropt == TRUE)
    return((char *) "");
  else if ((s = getenv("PBS_DPREFIX")) != NULL)
    return(s);
  else
    return(PBS_DPREFIX_DEFAULT);
  }

int
isexecutable(char *s)
  {
  char *c;

  c = s;

  if ((*c == ':') || ((*c == '#') && (*(c + 1) == '!'))) return FALSE;

  while (isspace(*c)) c++;

  if (notNULL(c)) return (*c != '#');

  return FALSE;
  }

char *
ispbsdir(char *s, char *prefix)
  {
  char *it;
  int l;

  it = s;

  while (isspace(*it)) it++;

  l = strlen(prefix);

  if (l > 0 && strncmp(it, prefix, l) == 0)
    return(it + l);
  else
    return((char *)NULL);
  }

void
make_argv(int *argc, char *argv[], char *line)
  {
  char *l, *b, *c;
  char buffer[4096];
  int len;
  char quote;

  *argc = 0;
  argv[(*argc)++] = strdup("scriptload");
  l = line;
  b = buffer;

  while (isspace(*l)) l++;

  c = l;

  while (*c != '\0')
    {
    if ((*c == '"') || (*c == '\''))
      {
      quote = *c;
      c++;

      while ((*c != quote) && *c)
        *b++ = *c++;

      if (*c == '\0')
        {
        fprintf(stderr, "scriptload: unmatched %c\n", *c);
        exit(1);
        }

      c++;
      }
    else if (*c == '\\')
      {
      c++;
      *b++ = *c++;
      }
    else if (isspace(*c))
      {
      len = c - l;

      if (argv[*argc] != NULL) free(argv[*argc]);

      argv[*argc] = (char *) malloc(len + 1);

      if (argv[*argc] == NULL)
        {
        fprintf(stderr, "scriptload: out of memory\n");
        exit(2);
        }

      *b = '\0';

      strcpy(argv[(*argc)++], buffer);

      while (isspace(*c)) c++;

      l = c;

      b = buffer;
      }
    else
      *b++ = *c++;
    }

  if (c != l)
    {
    len = c - l;

    if (argv[*argc] != NULL) free(argv[*argc]);

    argv[*argc] = (char *) malloc(len + 1);

    if (argv[*argc] == NULL)
      {
      fprintf(stderr, "scriptload: out of memory\n");
      exit(2);
      }

    *b = '\0';

    strcpy(argv[(*argc)++], buffer);
    }
  }


void print_attr_depend_options(

  std::vector<std::string> &dependency_options)

  {
  bool need_sep = false;

  if (dependency_options.size() == 0)
    return;

  printf("%s = ", ATTR_depend);
  // iterate over dependency_options
  for (std::vector<std::string>::const_iterator i = dependency_options.begin();
       i != dependency_options.end(); ++i)
    {
    if (need_sep)
      {
      printf(":");
      }
    else
      {
      need_sep = true;
      }

    printf("%s", (*i).c_str());
    }

  printf("\n");
  }


int process_opts(

  int    argc,
  char **argv,
  int    pass)

  {
  int i;
  int c;
  int errflg = 0;
  int passet;
  time_t after;
  char *keyword;
  char *valuewd;
  char *pc;

#if !defined(PBS_NO_POSIX_VIOLATION)
#define GETOPT_ARGS "a:A:c:C:e:hIj:k:l:m:M:N:o:p:q:r:S:u:v:VW:z"
#else
#define GETOPT_ARGS "a:A:c:C:e:hj:k:l:m:M:N:o:p:q:r:S:u:v:VW:z"
#endif /* PBS_NO_POSIX_VIOLATION */

#define MAX_RES_LIST_LEN 64

  /* The following macro, together the value of passet (pass + 1) is used */
  /* to enforce the following rules: 1. option on the command line take */
  /* precedence over those in script directives.   2. With in the command */
  /* line or within the script, the last occurance of an option takes */
  /* precedence over the earlier occurance.    */

#define if_cmd_line(x) if( (pass==0)||(x!=1) )

  passet = pass + 1;

  if (pass > 0)
    {
#ifdef linux
    optind = 0;  /* prime getopt's starting point */
#else
    optind = 1;  /* prime getopt's starting point */
#endif
    }

  while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    {
    switch (c)
      {

      case 'a':
        if_cmd_line(a_opt)
          {
          a_opt = passet;

          if ((after = cvtdate(optarg)) < 0)
            {
            fprintf(stderr, "scriptload: illegal -a value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_a, ctime(&after));
          }

        break;

      case 'A':
        if_cmd_line(A_opt)
          {
          A_opt = passet;
          printf("%s = %s\n", ATTR_A, optarg);
          }

        break;

      case 'c':
        if_cmd_line(c_opt)
          {
          c_opt = passet;

          while (isspace((int)*optarg)) optarg++;

          if (strlen(optarg) == 0)
            {
            fprintf(stderr, "scriptload: illegal -c value\n");
            errflg++;
            break;
            }

          pc = optarg;

          if (strlen(optarg) == 1)
            {
            if (*pc != 'n' && *pc != 's' && *pc != 'c')
              {
              fprintf(stderr, "scriptload: illegal -c value\n");
              errflg++;
              break;
              }
            }
          else
            {
            if (strncmp(optarg, "c=", 2) != 0)
              {
              fprintf(stderr, "scriptload: illegal -c value\n");
              errflg++;
              break;
              }

            pc += 2;

            if (*pc == '\0')
              {
              fprintf(stderr, "scriptload: illegal -c value\n");
              errflg++;
              break;
              }

            while (isdigit(*pc)) pc++;

            if (*pc != '\0')
              {
              fprintf(stderr, "scriptload: illegal -c value\n");
              errflg++;
              break;
              }
            }

          printf("%s = %s\n", ATTR_c, optarg);
          }

        break;

      case 'C':
        if_cmd_line(C_opt)
          {
          C_opt = passet;
          strcpy(dir_prefix, optarg);
          }

        break;

      case 'e':
        if_cmd_line(e_opt)
          {
          e_opt = passet;

          if (prepare_path(optarg, path_out, NULL) == 0)
            {
            printf("%s = %s\n", ATTR_e, path_out);
            }
          else
            {
            fprintf(stderr, "scriptload: illegal -e value\n");
            errflg++;
            }
          }

        break;

      case 'h':
        if_cmd_line(h_opt)
          {
          h_opt = passet;
          printf("%s = %s\n", ATTR_h, "u");
          }

        break;
#if !defined(PBS_NO_POSIX_VIOLATION)

      case 'I':
        if_cmd_line(Interact_opt)
          {
          Interact_opt = passet;
          }

        break;
#endif /* PBS_NO_POSIX_VIOLATION */

      case 'j':
        if_cmd_line(j_opt)
          {
          j_opt = passet;

          if (strcmp(optarg, "oe") != 0 &&
              strcmp(optarg, "eo") != 0 &&
              strcmp(optarg,  "n") != 0)
            {
            fprintf(stderr, "scriptload: illegal -j value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_j, optarg);
          }

        break;

      case 'k':
        if_cmd_line(k_opt)
          {
          k_opt = passet;

          if (strcmp(optarg,  "o") != 0 &&
              strcmp(optarg,  "e") != 0 &&
              strcmp(optarg, "oe") != 0 &&
              strcmp(optarg, "eo") != 0 &&
              strcmp(optarg,  "n") != 0)
            {
            fprintf(stderr, "scriptload: illegal -k value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_k, optarg);
          }

        break;

      case 'l':
        l_opt = passet;

        if (set_resources(&attrib, optarg, (pass == 0)))
          {
          fprintf(stderr, "scriptload: illegal -l value\n");
          errflg++;
          }

        printf("Resource_List = %s\n", optarg);

        break;

      case 'm':
        if_cmd_line(m_opt)
          {
          m_opt = passet;

          while (isspace((int)*optarg)) optarg++;

          if (strlen(optarg) == 0)
            {
            fprintf(stderr, "scriptload: illegal -m value\n");
            errflg++;
            break;
            }

          if (strcmp(optarg, "n") != 0)
            {
            pc = optarg;

            while (*pc)
              {
              if (*pc != 'a' && *pc != 'b' && *pc != 'e')
                {
                fprintf(stderr, "scriptload: illegal -m value\n");
                errflg++;
                break;
                }

              pc++;
              }
            }

          printf("%s = %s\n", ATTR_m, optarg);
          }

        break;

      case 'M':
        if_cmd_line(M_opt)
          {
          M_opt = passet;

          if (parse_at_list(optarg, FALSE, FALSE))
            {
            fprintf(stderr, "scriptload: illegal -M value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_M, optarg);
          }

        break;

      case 'N':
        if_cmd_line(N_opt)
          {
          N_opt = passet;

          if (check_job_name(optarg, 1) == 0)
            {
            printf("%s = %s\n", ATTR_N, optarg);
            }
          else
            {
            fprintf(stderr, "scriptload: illegal -N value\n");
            errflg++;
            }
          }

        break;

      case 'o':
        if_cmd_line(o_opt)
          {
          o_opt = passet;

          if (prepare_path(optarg, path_out, NULL) == 0)
            {
            printf("%s = %s\n", ATTR_o, path_out);
            }
          else
            {
            fprintf(stderr, "scriptload: illegal -o value\n");
            errflg++;
            }
          }

        break;

      case 'p':
        if_cmd_line(p_opt)
          {
          p_opt = passet;

          while (isspace((int)*optarg)) optarg++;

          pc = optarg;

          if (*pc == '-' || *pc == '+') pc++;

          if (strlen(pc) == 0)
            {
            fprintf(stderr, "scriptload: illegal -p value\n");
            errflg++;
            break;
            }

          while (*pc != '\0')
            {
            if (! isdigit(*pc))
              {
              fprintf(stderr, "scriptload: illegal -p value\n");
              errflg++;
              break;
              }

            pc++;
            }

          i = atoi(optarg);

          if (i < -1024 || i > 1023)
            {
            fprintf(stderr, "scriptload: illegal -p value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_p, optarg);
          }

        break;

      case 'q':
        if_cmd_line(q_opt)
          {
          q_opt = passet;
          strcpy(destination, optarg);
          printf("%s = %s\n", ATTR_queue, optarg);
          }

        break;

      case 'r':
        if_cmd_line(r_opt)
          {
          r_opt = passet;

          if (strlen(optarg) != 1)
            {
            fprintf(stderr, "scriptload: illegal -r value\n");
            errflg++;
            break;
            }

          if (*optarg != 'y' && *optarg != 'n')
            {
            fprintf(stderr, "scriptload: illegal -r value\n");
            errflg++;
            break;
            }

          if (*optarg == 'y')
            printf("%s = True\n", ATTR_r);
          else
            printf("%s = False\n", ATTR_r);

          }

        break;

      case 'S':
        if_cmd_line(S_opt)
          {
          S_opt = passet;

          if (parse_at_list(optarg, TRUE, TRUE))
            {
            fprintf(stderr, "scriptload: illegal -S value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_S, optarg);
          }

        break;

      case 'u':
        if_cmd_line(u_opt)
          {
          u_opt = passet;

          if (parse_at_list(optarg, TRUE, FALSE))
            {
            fprintf(stderr, "scriptload: illegal -u value\n");
            errflg++;
            break;
            }

          printf("%s = %s\n", ATTR_u, optarg);
          }

        break;

      case 'v':
        if_cmd_line(v_opt)
          {
          v_opt = passet;

          if (v_value != (char *)0) free(v_value);

          v_value = (char *) malloc(strlen(optarg) + 1);

          if (v_value == (char *)0)
            {
            fprintf(stderr, "scriptload: out of memory\n");
            errflg++;
            break;
            }

          strcpy(v_value, optarg);

          printf("Variable_List = %s\n", optarg);
          }

        break;

      case 'V':
        if_cmd_line(V_opt)
          {
          V_opt = passet;
          printf("Export_Current_Env = %d\n", TRUE);
          }

        break;

      case 'W':

        while (isspace((int)*optarg)) optarg++;

        if (strlen(optarg) == 0)
          {
          fprintf(stderr, "scriptload: illegal -W value\n");
          errflg++;
          break;
          }

        i = parse_equal_string(optarg, &keyword, &valuewd);

        while (i == 1)
          {
          if (strcmp(keyword, ATTR_depend) == 0)
            {
            if_cmd_line(Depend_opt)
              {
              Depend_opt = passet;

              std::vector<std::string> dependency_options;
              int rc;

              if (((rc = parse_depend_list(valuewd, dependency_options)) != PBSE_NONE) ||
                  (dependency_options.size() == 0))
                {
                /* cannot parse 'depend' value */
                if (rc == 2)
                  fprintf(stderr, "scriptload: -W value exceeded max length (%d)\n", PBS_DEPEND_LEN);
                else
                  fprintf(stderr, "scriptload: illegal -W dependency value: '%s'\n", valuewd);

                errflg++;

                break;
                }

              print_attr_depend_options(dependency_options);
              }
            }
          else if (strcmp(keyword, ATTR_stagein) == 0)
            {
            if_cmd_line(Stagein_opt)
              {
              Stagein_opt = passet;

              if (parse_stage_list(valuewd))
                {
                fprintf(stderr, "scriptload: illegal -W value\n");

                errflg++;

                break;
                }

              printf("%s = %s\n", ATTR_stagein, valuewd);
              }
            }
          else if (strcmp(keyword, ATTR_stageout) == 0)
            {
            if_cmd_line(Stageout_opt)
              {
              Stageout_opt = passet;

              if (parse_stage_list(valuewd))
                {
                fprintf(stderr, "scriptload: illegal -W value\n");
                errflg++;
                break;
                }

              printf("%s = %s\n", ATTR_stageout, valuewd);
              }
            }
          else if (strcmp(keyword, ATTR_g) == 0)
            {
            if_cmd_line(Grouplist_opt)
              {
              Grouplist_opt = passet;

              if (parse_at_list(valuewd, TRUE, FALSE))
                {
                fprintf(stderr, "scriptload: illegal -W value\n");
                errflg++;
                break;
                }

              printf("%s = %s\n", ATTR_g, valuewd);
              }
            }
          else if (strcmp(keyword, ATTR_inter) == 0)
            {
            if_cmd_line(Interact_opt)
              {
              Interact_opt = passet;

              if (strcmp(valuewd, "true") != 0)
                {
                fprintf(stderr, "scriptload: illegal -W value\n");
                errflg++;
                break;
                }
              }
            }
          else
            {
            set_attr(&attrib, keyword, valuewd);
            }

          i = parse_equal_string((char *)0, &keyword, &valuewd);
          }

        if (i == -1)
          {
          fprintf(stderr, "scriptload: illegal -W value\n");
          errflg++;
          }

        break;

      case 'z':
        if_cmd_line(z_opt)
          {
          z_opt = passet;
          }

        break;

      case '?':

      default :
        errflg++;
      }
    }

  if (!errflg && pass)
    {
    errflg = (optind != argc);

    if (errflg)
      {
      fprintf(stderr, "scriptload: directive error: ");

      for (optind = 1; optind < argc; optind++)
        fprintf(stderr, "%s ", argv[optind]);

      fprintf(stderr, "\n");
      }
    }

  return (errflg);
  }

int
do_dir(char *opts)
  {
  static int opt_pass = 1;
  int argc;

#define MAX_ARGV_LEN 128
  static char *vect[MAX_ARGV_LEN+1];

  if (opt_pass == 1)
    {
    argc = 0;

    while (argc < MAX_ARGV_LEN + 1) vect[argc++] = NULL;
    }

  make_argv(&argc, vect, opts);

  return process_opts(argc, vect, opt_pass++);
  }




int get_script(

  FILE *file,
  char *script,
  char *prefix)

  {
  char s[MAX_LINE_LEN+1];
  char *sopt;
  char *cont;
  int exec = FALSE;
  char tmp_name[] = "/tmp/qsub.XXXXXX";
  FILE *TMP_FILE;
  char *in;

  int tmpfd;


  if ((tmpfd = mkstemp(tmp_name)) < 0)
    {
    fprintf(stderr, "scriptload: could not create buffer file %s\n", tmp_name);

    return(4);
    }

  if ((TMP_FILE = fdopen(tmpfd, "w+")) == NULL)
    {
    fprintf(stderr, "scriptload: could not create buffer file %s\n", tmp_name);

    return(4);
    }

  while ((in = fgets(s, MAX_LINE_LEN, file)) != NULL)
    {
    if (!exec && ((sopt = ispbsdir(s, prefix)) != NULL))
      {
      while ((*(cont = in + strlen(in) - 2) == '\\') && (*(cont + 1) == '\n'))
        {
        /* next line is continuation of this line */

        in = cont;

        if ((in = fgets(in, MAX_LINE_LEN - (in - s), file)) == NULL)
          {
          fprintf(stderr, "scriptload: unexpected end-of-file or read error in script\n");

          fclose(TMP_FILE);

          return (6);
          }
        }   /* END while ( (*(cont = in + strlen(in) - 2) == '\\') && (*(cont+1) == '\n') ) */

      if (do_dir(sopt) != 0)
        {
        return (-1);
        }
      }    /* END if ( !exec && ((sopt = ispbsdir(s, prefix)) != NULL)) */
    else
      {
      if (fputs(in, TMP_FILE) < 0)
        {
        fprintf(stderr, "scriptload: error writing to buffer file, %s\n", tmp_name);

        fclose(TMP_FILE);

        return (3);
        }

      /* Stop processing PBS directive lines if we've reach the first executable line */

      if (!exec && isexecutable(s))
        {
        exec = TRUE;
        }
      }   /* END else if ( !exec && ((sopt = ispbsdir(s, prefix)) != NULL)) */
    }     /* END while ( (in = fgets(s, MAX_LINE_LEN, file)) != NULL ) */

  fclose(TMP_FILE);

  if (ferror(file))
    {
    fprintf(stderr, "scriptload: error reading script file\n");

    return(5);
    }
  else
    {
    strcpy(script, tmp_name);
    }

  return(0);
  }  /* END get_script() */



int
main(    /* scriptload */
  int argc,
  char **argv
)
  {
  int errflg;                         /* option error */
  static char script[MAXPATHLEN+1] = "";     /* name of script file */
  char script_tmp[MAXPATHLEN+1];      /* name of script containing non-PBS */
  /* lines */
  FILE *f;                            /* FILE pointer to the script */

  struct stat statbuf;

  errflg = process_opts(argc, argv, 0);  /* get cmd-line options */

  if (errflg || ((optind + 1) > argc))
    {
    static char usage[] = "usage: scriptload [-C directive_prefix] script\n";
    fprintf(stderr, "%s", usage);
    exit(2);
    }

  strcpy(script, argv[optind]);

  if (stat(script, &statbuf) < 0)
    {
    perror("scriptload: script file:");
    exit(1);
    }

  if (! S_ISREG(statbuf.st_mode))
    {
    fprintf(stderr, "scriptload: script not a file\n");
    exit(1);
    }

  if ((f = fopen(script, "r")) != NULL)
    {
    if ((errflg = get_script(f, script_tmp, set_dir_prefix(dir_prefix, C_opt))) > 0)
      {
      unlink(script_tmp);
      exit(1);
      }
    else if (errflg < 0)
      {
      exit(1);
      }
    }
  else
    {
    perror("scriptload: opening script file:");
    exit(8);
    }

  printf("Buffer_File = %s\n", script_tmp);

  exit(0);
  }
