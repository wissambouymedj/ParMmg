/* =============================================================================
**  This file is part of the parmmg software package for the tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/Inria/InriaSoft, 2016- .
**
**  parmmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  parmmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with parmmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the parmmg distribution only if you accept them.
** =============================================================================
*/

/****************************************
 * genheader.c
 *
 * generate an executable to generate
 * headers for Fortran parmmg users.
 * Used by makefile
 ****************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/*
  Program: genheader

  Generate headers (for fortran) setting MMG5_DATA_PTR
  macro to the correct value.

  Parameters:
  argc - must be 3
  argv - contains the names wanted for the Fortran header

  Returns:
  EXIT_SUCCESS

*/
int main (int argc, char ** argv)
{
  char * header_f          = NULL;
  char * libparmmg_h       = NULL;
  char * libparmmg_include = NULL;
  char * genfort           = NULL;
  char * cmd               = NULL;
  FILE * file              = NULL;

  if (argc != 5)
    {
      fprintf(stderr, "usage : %s headerNameFortran.h headerNameC.h"
              " libparmmgtypesf_include_dir genfort.pl\n",argv[0]);
      return EXIT_FAILURE;
    }

  header_f          = argv[1];
  libparmmg_h       = argv[2];
  libparmmg_include = argv[3];
  genfort           = argv[4];

  /* Fortran header */
  file = fopen (header_f,"w");

  if ( file == NULL ) {
    return(EXIT_FAILURE);
  }

  fprintf(file,"! /* This file is automatically generated by the"
          "\"genheader.c\" code\n");
  fprintf(file,"! ** and the \"genfort.pl\" script (scripts directory).\n");
  fprintf(file,"! ** Do not modified it by hand, it will be discarded.\n");
  fprintf(file,"! **\n");
  fprintf(file,"! ** This scripts recopy the macros of the libparmmg*.h file\n");
  fprintf(file,"! ** and convert enumerations into macros.\n");
  fprintf(file,"! **\n");
  fprintf(file,"! ** Note: \"genfort.pl\" is automatically called by the ");
  fprintf(file,"\"genheader.c\"\n! ** executable code.\n");
  fprintf(file,"! **\n");
  fprintf(file,"! ** See the \"libparmmg*.h\" file for a correct displaying of"
          " the documentation.\n");
  fprintf(file,"! */\n\n");

  /* Include libparmmgtypesf.h if needed. */
  if(strstr(libparmmg_h,"libparmmgtypes.h")==NULL) {
    fprintf(file,"#include \"mpif.h\"\n\n");
    fprintf(file,"#include \"%s/libparmmgtypesf.h\"\n\n",libparmmg_include);
  }
  else {
    fprintf(file,"#include \"mmg/mmg3d/libmmgtypesf.h\"\n\n");
  }
  fclose(file);

  /* Generate Fortran header */
  if (NULL == (cmd = (char*)malloc((strlen(genfort)+
                                    strlen(libparmmg_h)+
                                    strlen(header_f)+128)*sizeof(char))))
    return EXIT_FAILURE;
  sprintf(cmd, "perl %s -f %s >> %s;",
          genfort, libparmmg_h, header_f);
  fprintf(stdout, "%s\n", cmd);
  if (-1 == system(cmd))
    return EXIT_FAILURE;
  free(cmd);

  return(0);
}
