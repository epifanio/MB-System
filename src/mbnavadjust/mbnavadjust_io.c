/*--------------------------------------------------------------------
 *    The MB-system:  mbnavadjust_io.c  3/23/00
 *
 *    Copyright (c) 2014-2020 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, CA 95039
 *    and Dale N. Chayes (dale@ldeo.columbia.edu)
 *      Lamont-Doherty Earth Observatory
 *      Palisades, NY 10964
 *
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/*
 * mbnavadjust is an interactive navigation adjustment package
 * for swath sonar data.
 * It can work with any data format supported by the MBIO library.
 * This file contains the code that does not directly depend on the
 * MOTIF interface.
 *
 * Author:  D. W. Caress
 * Date:  April 14, 2014
 */

/*--------------------------------------------------------------------*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mb_aux.h"
#include "mb_define.h"
#include "mb_io.h"
#include "mb_process.h"
#include "mb_status.h"
#include "mbnavadjust_io.h"
#include "mbsys_ldeoih.h"

/* get NaN detector */
#if defined(isnanf)
#define check_fnan(x) isnanf((x))
#elif defined(isnan)
#define check_fnan(x) isnan((double)(x))
#elif HAVE_ISNANF == 1
#define check_fnan(x) isnanf(x)
extern int isnanf(float x);
#elif HAVE_ISNAN == 1
#define check_fnan(x) isnan((double)(x))
#elif HAVE_ISNAND == 1
#define check_fnan(x) isnand((double)(x))
#else
#define check_fnan(x) ((x) != (x))
#endif

static const char program_name[] = "mbnavadjust i/o functions";

/*--------------------------------------------------------------------*/
int mbnavadjust_new_project(int verbose, char *projectpath, double section_length, int section_soundings, double cont_int,
                            double col_int, double tick_int, double label_int, int decimation, double smoothing,
                            double zoffsetwidth, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:            %d\n", verbose);
    fprintf(stderr, "dbg2       projectpath:        %s\n", projectpath);
    fprintf(stderr, "dbg2       section_length:     %f\n", section_length);
    fprintf(stderr, "dbg2       section_soundings:  %d\n", section_soundings);
    fprintf(stderr, "dbg2       cont_int:           %f\n", cont_int);
    fprintf(stderr, "dbg2       col_int:            %f\n", col_int);
    fprintf(stderr, "dbg2       tick_int:           %f\n", tick_int);
    fprintf(stderr, "dbg2       label_int:          %f\n", label_int);
    fprintf(stderr, "dbg2       decimation:         %d\n", decimation);
    fprintf(stderr, "dbg2       smoothing:          %f\n", smoothing);
    fprintf(stderr, "dbg2       zoffsetwidth:       %f\n", zoffsetwidth);
    fprintf(stderr, "dbg2       project:            %p\n", project);
  }

  /* if project structure holds an open project close it first */
  int status = MB_SUCCESS;
  if (project->open)
    status = mbnavadjust_close_project(verbose, project, error);

  /* check path to see if new project can be created */
  char *nameptr = NULL;
  char *slashptr = strrchr(projectpath, '/');
  if (slashptr != NULL)
    nameptr = slashptr + 1;
  else
    nameptr = projectpath;
  if (strlen(nameptr) > 4 && strcmp(&nameptr[strlen(nameptr) - 4], ".nvh") == 0)
    nameptr[strlen(nameptr) - 4] = '\0';
  if (strlen(nameptr) == 0) {
    fprintf(stderr, "Unable to create new project!\nInvalid project path: %s\n", projectpath);
    *error = MB_ERROR_INIT_FAIL;
    status = MB_FAILURE;
  }

  /* try to create new project */
  if (status == MB_SUCCESS) {
    strcpy(project->name, nameptr);
    if (strlen(projectpath) == strlen(nameptr)) {
      /* char *result = */ getcwd(project->path, MB_PATH_MAXLINE);
      strcat(project->path, "/");
    }
    else {
      strncpy(project->path, projectpath, strlen(projectpath) - strlen(nameptr));
    }
    strcpy(project->home, project->path);
    strcat(project->home, project->name);
    strcat(project->home, ".nvh");
    strcpy(project->datadir, project->path);
    strcat(project->datadir, project->name);
    strcat(project->datadir, ".dir");
    strcpy(project->logfile, project->datadir);
    strcat(project->logfile, "/log.txt");

    /* no new project if file or directory already exist */
    struct stat statbuf;
    if (stat(project->home, &statbuf) == 0) {
      fprintf(stderr, "Unable to create new project!\nHome file %s already exists\n", project->home);
      *error = MB_ERROR_INIT_FAIL;
      status = MB_FAILURE;
    }
    if (stat(project->datadir, &statbuf) == 0) {
      fprintf(stderr, "Unable to create new project!\nData directory %s already exists\n", project->datadir);
      *error = MB_ERROR_INIT_FAIL;
      status = MB_FAILURE;
    }

    /* initialize new project */
    if (status == MB_SUCCESS) {
      /* set values */
      project->open = true;
      project->num_files = 0;
      project->num_files_alloc = 0;
      project->files = NULL;
      project->num_snavs = 0;
      project->num_pings = 0;
      project->num_beams = 0;
      project->num_crossings = 0;
      project->num_crossings_alloc = 0;
      project->num_crossings_analyzed = 0;
      project->num_goodcrossings = 0;
      project->num_truecrossings = 0;
      project->num_truecrossings_analyzed = 0;
      project->crossings = NULL;
      project->num_ties = 0;
      project->section_length = section_length;
      project->section_soundings = section_soundings;
      project->cont_int = cont_int;
      project->col_int = col_int;
      project->tick_int = tick_int;
      project->label_int = label_int;
      project->decimation = decimation;
      project->precision = SIGMA_MINIMUM;
      project->smoothing = smoothing;
      project->zoffsetwidth = zoffsetwidth;
      project->inversion_status = MBNA_INVERSION_NONE;
      project->grid_status = MBNA_GRID_NONE;
      project->modelplot = false;
      project->modelplot_style = MBNA_MODELPLOT_TIMESERIES;
      project->modelplot_uptodate = false;
      project->logfp = NULL;
      project->precision = SIGMA_MINIMUM;
      project->smoothing = MBNA_SMOOTHING_DEFAULT;

      /* create data directory */
#ifdef _WIN32
      if (mkdir(project->datadir) != 0) {
#else
      if (mkdir(project->datadir, 00775) != 0) {
#endif
        fprintf(stderr, "Error creating data directory %s\n", project->datadir);
        *error = MB_ERROR_INIT_FAIL;
        status = MB_FAILURE;
      }

      /* write home file and other files */
      else if ((status = mbnavadjust_write_project(verbose, project, error)) == MB_FAILURE) {
        fprintf(stderr, "Failure to write project file %s\n", project->home);
        *error = MB_ERROR_INIT_FAIL;
        status = MB_FAILURE;
      }

      /* initialize log file */
      else if ((project->logfp = fopen(project->logfile, "w")) == NULL) {
        fprintf(stderr, "Failure to create log file %s\n", project->logfile);
        *error = MB_ERROR_INIT_FAIL;
        status = MB_FAILURE;
      }

      /* first message in log file */
      else {
        fprintf(project->logfp, "New project initialized: %s\n > Project home: %s\n", project->name, project->home);
      }
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_read_project(int verbose, char *projectpath, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:            %d\n", verbose);
    fprintf(stderr, "dbg2       projectname:        %s\n", projectpath);
    fprintf(stderr, "dbg2       project:            %p\n", project);
  }

  int status = MB_SUCCESS;

  /* if project structure holds an open project close it first */
  if (project->open)
    status = mbnavadjust_close_project(verbose, project, error);

  /* check path to see if project exists */
  char *slashptr = strrchr(projectpath, '/');
  char *nameptr = slashptr != NULL ?slashptr + 1 :projectpath;
  if (strlen(nameptr) > 4 && strcmp(&nameptr[strlen(nameptr) - 4], ".nvh") == 0)
    nameptr[strlen(nameptr) - 4] = '\0';
  if (strlen(nameptr) == 0) {
    fprintf(stderr, "Unable to read project!\nInvalid project path: %s\n", projectpath);
    *error = MB_ERROR_INIT_FAIL;
    status = MB_FAILURE;
  }

  /* try to read project */
  if (status == MB_SUCCESS) {
    strcpy(project->name, nameptr);
    char *result;
    if (strlen(projectpath) == strlen(nameptr)) {
      result = getcwd(project->path, MB_PATH_MAXLINE);
      strcat(project->path, "/");
    }
    else {
      strcpy(project->path, projectpath);
      project->path[strlen(projectpath) - strlen(nameptr)] = '\0';
    }
    strcpy(project->home, project->path);
    strcat(project->home, project->name);
    strcat(project->home, ".nvh");
    strcpy(project->datadir, project->path);
    strcat(project->datadir, project->name);
    strcat(project->datadir, ".dir");
    strcpy(project->logfile, project->datadir);
    strcat(project->logfile, "/log.txt");

    /* check if project exists */
    struct stat statbuf;
    if (stat(project->home, &statbuf) != 0) {
      fprintf(stderr, "Project home file %s does not exist\n", project->home);
      *error = MB_ERROR_INIT_FAIL;
      status = MB_FAILURE;
    }
    if (stat(project->datadir, &statbuf) != 0) {
      fprintf(stderr, "Data directory %s does not exist\n", project->datadir);
      *error = MB_ERROR_INIT_FAIL;
      status = MB_FAILURE;
    }

    /* read the project */
    if (status == MB_SUCCESS) {
      /* first save copy of the project file */
      char command[MB_PATH_MAXLINE];
      sprintf(command, "cp %s %s.save", project->home, project->home);
      /* const int shellstatus = */ system(command);

      /* open and read home file */
      status = MB_SUCCESS;
      FILE *hfp = fopen(project->home, "r");
      if (hfp != NULL) {
        char buffer[BUFFER_MAX];

        /* check for proper header */
        if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer || strncmp(buffer, "##MBNAVADJUST PROJECT", 21) != 0)
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        /* read basic names and stats */
        int nscan;
        char label[STRING_MAX];
        char obuffer[BUFFER_MAX];
        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "MB-SYSTEM_VERSION") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "PROGRAM_VERSION") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        int versionmajor;
        int versionminor;
        if (status == MB_SUCCESS && ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                                     (nscan = sscanf(buffer, "%s %d.%d", label, &versionmajor, &versionminor)) != 3 ||
                                     strcmp(label, "FILE_VERSION") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }
        const int version_id = 100 * versionmajor + versionminor;

        if (version_id >= 302) {
          if (status == MB_SUCCESS &&
              ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
               (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "ORIGIN") != 0))
            status = MB_FAILURE;
        }
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "NAME") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "PATH") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "HOME") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %s", label, obuffer)) != 2 || strcmp(label, "DATADIR") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %d", label, &project->num_files)) != 2 || strcmp(label, "NUMFILES") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (version_id >= 306) {
          if (status == MB_SUCCESS &&
              ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
               (nscan = sscanf(buffer, "%s %d", label, &project->num_surveys)) != 2 || strcmp(label, "NUMBLOCKS") != 0))
            status = MB_FAILURE;
        }
        else {
          project->num_surveys = 0;
        }
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS && ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                                     (nscan = sscanf(buffer, "%s %d", label, &project->num_crossings)) != 2 ||
                                     strcmp(label, "NUMCROSSINGS") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS && ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                                     (nscan = sscanf(buffer, "%s %lf", label, &project->section_length)) != 2 ||
                                     strcmp(label, "SECTIONLENGTH") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS && version_id >= 101 &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %d", label, &project->section_soundings)) != 2 ||
             strcmp(label, "SECTIONSOUNDINGS") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %d", label, &project->decimation)) != 2 || strcmp(label, "DECIMATION") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %lf", label, &project->cont_int)) != 2 || strcmp(label, "CONTOURINTERVAL") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %lf", label, &project->col_int)) != 2 || strcmp(label, "COLORINTERVAL") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS &&
            ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
             (nscan = sscanf(buffer, "%s %lf", label, &project->tick_int)) != 2 || strcmp(label, "TICKINTERVAL") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS && ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                                     (nscan = sscanf(buffer, "%s %d", label, &project->inversion_status)) != 2 ||
                                     strcmp(label, "INVERSION") != 0))
          status = MB_FAILURE;
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s buffer:%s\n", __LINE__, __FILE__, buffer);
          exit(0);
        }

        if (status == MB_SUCCESS) {
          if (version_id >= 307) {
            if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                (nscan = sscanf(buffer, "%s %d", label, &project->grid_status)) != 2 ||
                strcmp(label, "GRIDSTATUS") != 0)
              status = MB_FAILURE;
          }
        }
        if (status == MB_SUCCESS) {
          if (version_id >= 301) {
            if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                (nscan = sscanf(buffer, "%s %lf", label, &project->smoothing)) != 2 ||
                strcmp(label, "SMOOTHING") != 0)
              status = MB_FAILURE;
            project->precision = SIGMA_MINIMUM;
          }
          else if (version_id >= 103) {
            if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                (nscan = sscanf(buffer, "%s %lf", label, &project->precision)) != 2 ||
                strcmp(label, "PRECISION") != 0)
              status = MB_FAILURE;
            project->smoothing = MBNA_SMOOTHING_DEFAULT;
          }
          else {
            project->precision = SIGMA_MINIMUM;
            project->smoothing = MBNA_SMOOTHING_DEFAULT;
          }
        }
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s\n", __LINE__, __FILE__);
          exit(0);
        }

        if (status == MB_SUCCESS) {
          if (version_id >= 105) {
            if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                (nscan = sscanf(buffer, "%s %lf", label, &project->zoffsetwidth)) != 2 ||
                strcmp(label, "ZOFFSETWIDTH") != 0)
              status = MB_FAILURE;
          }
          else
            project->zoffsetwidth = 5.0;
        }

        /* allocate memory for files array */
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s\n", __LINE__, __FILE__);
          exit(0);
        }

        if (project->num_files > 0) {
          project->files = (struct mbna_file *)malloc(sizeof(struct mbna_file) * (project->num_files));
          if (project->files != NULL) {
            project->num_files_alloc = project->num_files;
            memset(project->files, 0, project->num_files_alloc * sizeof(struct mbna_file));
          }
          else {
            project->num_files_alloc = 0;
            status = MB_FAILURE;
            *error = MB_ERROR_MEMORY_FAIL;
          }
        }
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s\n", __LINE__, __FILE__);
          exit(0);
        }

        if (project->num_crossings > 0) {
          project->crossings = (struct mbna_crossing *)malloc(sizeof(struct mbna_crossing) * (project->num_crossings));
          if (project->crossings != NULL) {
            project->num_crossings_alloc = project->num_crossings;
            memset(project->crossings, 0, sizeof(struct mbna_crossing) * project->num_crossings_alloc);
          }
          else {
            project->num_crossings_alloc = 0;
            status = MB_FAILURE;
            *error = MB_ERROR_MEMORY_FAIL;
          }
        }
        if (status == MB_FAILURE) {
          fprintf(stderr, "Die at line:%d file:%s\n", __LINE__, __FILE__);
          exit(0);
        }

        bool first;
        int s1id;
        int s2id;
        struct mbna_section *section, *section1, *section2;
        int idummy;
        int k, l;

        for (int ifile = 0; ifile < project->num_files; ifile++) {
          struct mbna_file *file = &project->files[ifile];
          file->num_sections_alloc = 0;
          file->sections = NULL;
          file->num_snavs = 0;
          file->num_pings = 0;
          file->num_beams = 0;
          if (version_id >= 306) {
            if (status == MB_SUCCESS &&
                ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                 (nscan = sscanf(buffer, "FILE %d %d %d %d %d %lf %lf %lf %lf %lf %lf %lf %d %d %s", &idummy,
                                 &(file->status), &(file->id), &(file->format), &(file->block),
                                 &(file->block_offset_x), &(file->block_offset_y), &(file->block_offset_z),
                                 &(file->heading_bias_import), &(file->roll_bias_import), &(file->heading_bias),
                                 &(file->roll_bias), &(file->num_sections), &(file->output_id), file->file)) != 15))
              status = MB_FAILURE;
          }
          else {
            if (status == MB_SUCCESS &&
                ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                 (nscan = sscanf(buffer, "FILE %d %d %d %d %lf %lf %lf %lf %d %d %s", &idummy, &(file->status),
                                 &(file->id), &(file->format), &(file->heading_bias_import),
                                 &(file->roll_bias_import), &(file->heading_bias), &(file->roll_bias),
                                 &(file->num_sections), &(file->output_id), file->file)) != 11))
              status = MB_FAILURE;
            file->block = 0;
            file->block_offset_x = 0.0;
            file->block_offset_y = 0.0;
            file->block_offset_z = 0.0;
          }

          /* set file->path as absolute path
              - file->file may be a relative path */
          if (status == MB_SUCCESS) {
            if (file->file[0] == '/')
              strcpy(file->path, file->file);
            else {
              strcpy(file->path, project->path);
              strcat(file->path, file->file);
            }
          }

          /* reset output_id to 0 - this should no longer have any value but 0 */
          file->output_id = 0;

          /* read section info */
          if (file->num_sections > 0) {
            file->sections = (struct mbna_section *)malloc(sizeof(struct mbna_section) * (file->num_sections));
            if (file->sections != NULL) {
              file->num_sections_alloc = file->num_sections;
              memset(file->sections, 0, sizeof(struct mbna_section) * file->num_sections_alloc);
            }
            else {
              file->num_sections_alloc = 0;
              status = MB_FAILURE;
              *error = MB_ERROR_MEMORY_FAIL;
            }
          }
          for (int isection = 0; isection < file->num_sections; isection++) {
            section = &file->sections[isection];
            if (status == MB_SUCCESS)
              result = fgets(buffer, BUFFER_MAX, hfp);
            if (status == MB_SUCCESS && result == buffer)
              nscan = sscanf(buffer, "SECTION %d %d %d %d %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %d", &idummy,
                             &section->num_pings, &section->num_beams, &section->num_snav, &section->continuity,
                             &section->distance, &section->btime_d, &section->etime_d, &section->lonmin,
                             &section->lonmax, &section->latmin, &section->latmax, &section->depthmin,
                             &section->depthmax, &section->contoursuptodate);
            if (result != buffer || nscan < 14) {
              status = MB_FAILURE;
              fprintf(stderr, "read failed on section: %s\n", buffer);
            }
            if (nscan < 15)
              section->contoursuptodate = false;
            for (k = MBNA_MASK_DIM - 1; k >= 0; k--) {
              if (status == MB_SUCCESS)
                result = fgets(buffer, BUFFER_MAX, hfp);
              for (l = 0; l < MBNA_MASK_DIM; l++) {
                sscanf(&buffer[l], "%1d", &section->coverage[l + k * MBNA_MASK_DIM]);
              }
            }
            if (status == MB_FAILURE) {
              fprintf(stderr, "Die at line:%d file:%s\n", __LINE__, __FILE__);
              exit(0);
            }
            /*fprintf(stderr,"%s/nvs_%4.4d_%4.4d.mb71\n",
            project->datadir,file->id,isection);
            for (k=MBNA_MASK_DIM-1;k>=0;k--)
            {
            for (l=0;l<MBNA_MASK_DIM;l++)
            {
            fprintf(stderr, "%1d", section->coverage[l + k * MBNA_MASK_DIM]);
            }
            fprintf(stderr, "\n");
            }*/
            for (k = 0; k < section->num_snav; k++) {
              if (status == MB_SUCCESS)
                result = fgets(buffer, BUFFER_MAX, hfp);
                if (version_id >= 308) {
                    if (status == MB_SUCCESS && result == buffer)
                        nscan = sscanf(buffer, "SNAV %d %d %lf %lf %lf %lf %lf %lf %lf %lf",
                                       &idummy, &section->snav_id[k], &section->snav_distance[k], &section->snav_time_d[k],
                                       &section->snav_lon[k], &section->snav_lat[k], &section->snav_sensordepth[k],
                                       &section->snav_lon_offset[k], &section->snav_lat_offset[k],
                                       &section->snav_z_offset[k]);
                    section->snav_num_ties[k] = 0;
                    if (section->snav_sensordepth[k] < 0.0 || section->snav_sensordepth[k] > 11000.0)
                      section->snav_sensordepth[k] = 0.0;
                    if (result != buffer || nscan != 10) {
                        status = MB_FAILURE;
                        fprintf(stderr, "read failed on snav: %s\n", buffer);
                    }
                }
                else {
                    if (status == MB_SUCCESS && result == buffer)
                        nscan = sscanf(buffer, "SNAV %d %d %lf %lf %lf %lf %lf %lf %lf", &idummy, &section->snav_id[k],
                                       &section->snav_distance[k], &section->snav_time_d[k], &section->snav_lon[k],
                                       &section->snav_lat[k], &section->snav_lon_offset[k], &section->snav_lat_offset[k],
                                       &section->snav_z_offset[k]);
                    section->snav_num_ties[k] = 0;
                    section->snav_sensordepth[k] = 0.0;
                    if (result == buffer && nscan == 6) {
                        section->snav_lon_offset[k] = 0.0;
                        section->snav_lat_offset[k] = 0.0;
                        section->snav_z_offset[k] = 0.0;
                    }
                    else if (result == buffer && nscan == 8) {
                        section->snav_z_offset[k] = 0.0;
                    }
                    else if (result != buffer || nscan != 9) {
                        status = MB_FAILURE;
                        fprintf(stderr, "read failed on snav: %s\n", buffer);
                    }

                    /* reverse offset values if older values */
                    if (version_id < 300) {
                        section->snav_lon_offset[k] *= -1.0;
                        section->snav_lat_offset[k] *= -1.0;
                        section->snav_z_offset[k] *= -1.0;
                    }
                }
            }

            /* global fixed frame tie, whether defined or not */
            section->global_tie_status = MBNA_TIE_NONE;
            section->global_tie_snav = MBNA_SELECT_NONE;
            section->global_tie_inversion_status = MBNA_INVERSION_NONE;
            section->offset_x = 0.0;
            section->offset_y = 0.0;
            section->offset_x_m = 0.0;
            section->offset_y_m = 0.0;
            section->offset_z_m = 0.0;
            section->xsigma = 0.0;
            section->ysigma = 0.0;
            section->zsigma = 0.0;
            section->inversion_offset_x = 0.0;
            section->inversion_offset_y = 0.0;
            section->inversion_offset_x_m = 0.0;
            section->inversion_offset_y_m = 0.0;
            section->inversion_offset_z_m = 0.0;
            section->dx_m = 0.0;
            section->dy_m = 0.0;
            section->dz_m = 0.0;
            section->sigma_m = 0.0;
            section->dr1_m = 0.0;
            section->dr2_m = 0.0;
            section->dr3_m = 0.0;
            section->rsigma_m = 0.0;

            if (version_id >= 309) {
              if (status == MB_SUCCESS)
                result = fgets(buffer, BUFFER_MAX, hfp);
              if (status == MB_SUCCESS && result == buffer)
                nscan = sscanf(buffer, "GLOBALTIE %d %d %d %lf %lf %lf %lf %lf %lf",
                                                &section->global_tie_status, &section->global_tie_snav,
                                                &section->global_tie_inversion_status,
                                                &section->offset_x, &section->offset_y, &section->offset_z_m,
                                                &section->xsigma, &section->ysigma, &section->zsigma);
            }
            else if (version_id >= 305) {
              if (status == MB_SUCCESS)
                result = fgets(buffer, BUFFER_MAX, hfp);
              if (status == MB_SUCCESS && result == buffer)
                nscan = sscanf(buffer, "GLOBALTIE %d %d %lf %lf %lf %lf %lf %lf",
                                                &section->global_tie_status, &section->global_tie_snav,
                                                &section->offset_x, &section->offset_y, &section->offset_z_m,
                                                &section->xsigma, &section->ysigma, &section->zsigma);
              if (section->global_tie_status != MBNA_TIE_NONE) {
                                section->global_tie_inversion_status = project->inversion_status;
              }
            }
            else if (version_id == 304) {
              if (status == MB_SUCCESS)
                result = fgets(buffer, BUFFER_MAX, hfp);
              if (status == MB_SUCCESS && result == buffer)
                nscan = sscanf(buffer, "GLOBALTIE %d %lf %lf %lf %lf %lf %lf",
                                                &section->global_tie_snav,
                                                &section->offset_x, &section->offset_y, &section->offset_z_m,
                                                &section->xsigma, &section->ysigma, &section->zsigma);
              if (section->global_tie_snav != MBNA_SELECT_NONE) {
                section->global_tie_status = MBNA_TIE_XYZ;
                                section->global_tie_inversion_status = project->inversion_status;
               }
            }

            if (section->global_tie_status == 0 && section->global_tie_snav == -1) {
              section->global_tie_inversion_status = 0;
              section->offset_x = 0.0;
              section->offset_y = 0.0;
              section->offset_z_m = 0.0;
              section->xsigma = 0.0;
              section->ysigma = 0.0;
              section->zsigma = 0.0;
            }
          }
        }

        /* set project bounds and scaling */
        first = true;
        for (int ifile = 0; ifile < project->num_files; ifile++) {
          struct mbna_file *file = &project->files[ifile];
          if (file->status != MBNA_FILE_FIXEDNAV) {
            for (int isection = 0; isection < file->num_sections; isection++) {
              section = &file->sections[isection];
              if (!(check_fnan(section->lonmin) || check_fnan(section->lonmax) || check_fnan(section->latmin) ||
                    check_fnan(section->latmax))) {
                if (first) {
                  project->lon_min = section->lonmin;
                  project->lon_max = section->lonmax;
                  project->lat_min = section->latmin;
                  project->lat_max = section->latmax;
                  first = false;
                }
                else {
                  project->lon_min = MIN(project->lon_min, section->lonmin);
                  project->lon_max = MAX(project->lon_max, section->lonmax);
                  project->lat_min = MIN(project->lat_min, section->latmin);
                  project->lat_max = MAX(project->lat_max, section->latmax);
                }
              }
            }
          }
          // fprintf(stderr, "PROJECT BOUNDS: file %d %s: %.7f %.7f    %.7f %.7f\n",
          // ifile, file->path, project->lon_min, project->lon_max, project->lat_min, project->lat_max);
        }
        mb_coor_scale(verbose, 0.5 * (project->lat_min + project->lat_max), &project->mtodeglon, &project->mtodeglat);

        /* add sensordepth values to snav lists if needed */
                if (version_id < 308) {
fprintf(stderr,"Project version %d previous to 3.08: Adding sensordepth values to section snav arrays...\n", version_id);
                    status = mbnavadjust_fix_section_sensordepth(verbose, project, error);
                }

        /* recount the number of blocks */
        project->num_surveys = 0;
        for (int ifile = 0; ifile < project->num_files; ifile++) {
          struct mbna_file *file = &project->files[ifile];
          if (ifile == 0 || !file->sections[0].continuity) {
            project->num_surveys++;
          }
          file->block = project->num_surveys - 1;
          file->block_offset_x = 0.0;
          file->block_offset_y = 0.0;
          file->block_offset_z = 0.0;
        }

        /* now do scaling of global ties since mtodeglon and mtodeglat are defined */
        for (int ifile = 0; ifile < project->num_files; ifile++) {
          struct mbna_file *file = &project->files[ifile];
          for (int isection = 0; isection < file->num_sections; isection++) {
            section = &file->sections[isection];
                        if (section->global_tie_status != MBNA_TIE_NONE) {
                            section->offset_x_m = section->offset_x / project->mtodeglon;
                            section->offset_y_m = section->offset_y / project->mtodeglat;
                            if (section->global_tie_inversion_status != MBNA_INVERSION_NONE) {
                                section->inversion_offset_x = section->snav_lon_offset[section->global_tie_snav];
                                section->inversion_offset_y = section->snav_lat_offset[section->global_tie_snav];
                                section->inversion_offset_x_m = section->snav_lon_offset[section->global_tie_snav] / project->mtodeglon;
                                section->inversion_offset_y_m = section->snav_lat_offset[section->global_tie_snav] / project->mtodeglat;
                                section->inversion_offset_z_m = section->snav_z_offset[section->global_tie_snav];
                                section->dx_m = section->offset_x_m - section->inversion_offset_x_m;
                                section->dy_m = section->offset_y_m - section->inversion_offset_y_m;
                                section->dz_m = section->offset_z_m - section->inversion_offset_z_m;
                                section->sigma_m = sqrt(section->dx_m * section->dx_m + section->dy_m * section->dy_m + section->dz_m * section->dz_m);
                                section->dr1_m = section->inversion_offset_x_m / section->xsigma;
                                section->dr2_m = section->inversion_offset_y_m / section->ysigma;
                                section->dr3_m = section->inversion_offset_z_m / section->zsigma;
                                section->rsigma_m = sqrt(section->dr1_m * section->dr1_m + section->dr2_m * section->dr2_m + section->dr3_m * section->dr3_m);
                            }
                        }
                    }
                }

        /* read crossings */
        project->num_crossings_analyzed = 0;
        project->num_goodcrossings = 0;
        project->num_truecrossings = 0;
        project->num_truecrossings_analyzed = 0;
        project->num_ties = 0;
        for (int icrossing = 0; icrossing < project->num_crossings; icrossing++) {
          /* read each crossing */
          struct mbna_crossing *crossing = &project->crossings[icrossing];
          if (status == MB_SUCCESS && version_id >= 106) {
            if (status == MB_SUCCESS &&
                ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                 (nscan =
                      sscanf(buffer, "CROSSING %d %d %d %d %d %d %d %d %d", &idummy, &crossing->status,
                             &crossing->truecrossing, &crossing->overlap, &crossing->file_id_1, &crossing->section_1,
                             &crossing->file_id_2, &crossing->section_2, &crossing->num_ties)) != 9)) {
              status = MB_FAILURE;
              fprintf(stderr, "read failed on crossing: %s\n", buffer);
            }
          }
          else if (status == MB_SUCCESS && version_id >= 102) {
            crossing->overlap = 0;
            if (status == MB_SUCCESS &&
                ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                 (nscan = sscanf(buffer, "CROSSING %d %d %d %d %d %d %d %d", &idummy, &crossing->status,
                                 &crossing->truecrossing, &crossing->file_id_1, &crossing->section_1,
                                 &crossing->file_id_2, &crossing->section_2, &crossing->num_ties)) != 8)) {
              status = MB_FAILURE;
              fprintf(stderr, "read failed on crossing: %s\n", buffer);
            }
          }
          else if (status == MB_SUCCESS) {
            crossing->truecrossing = false;
            crossing->overlap = 0;
            if (status == MB_SUCCESS &&
                ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                 (nscan = sscanf(buffer, "CROSSING %d %d %d %d %d %d %d", &idummy, &crossing->status,
                                 &crossing->file_id_1, &crossing->section_1, &crossing->file_id_2,
                                 &crossing->section_2, &crossing->num_ties)) != 7)) {
              status = MB_FAILURE;
              fprintf(stderr, "read failed on old format crossing: %s\n", buffer);
            }
          }
          if (status == MB_SUCCESS && crossing->status != MBNA_CROSSING_STATUS_NONE)
            project->num_crossings_analyzed++;
          if (status == MB_SUCCESS && crossing->truecrossing) {
            project->num_truecrossings++;
            if (crossing->status != MBNA_CROSSING_STATUS_NONE)
              project->num_truecrossings_analyzed++;
          }

          /* reorder crossing to be early file first older file second if
                  file version prior to 3.00 */
          if (version_id < 300) {
            idummy = crossing->file_id_1;
            int jdummy = crossing->section_1;
            crossing->file_id_1 = crossing->file_id_2;
            crossing->section_1 = crossing->section_2;
            crossing->file_id_2 = idummy;
            crossing->section_2 = jdummy;
          }

          /* read ties */
          if (status == MB_SUCCESS) {
            for (int itie = 0; itie < crossing->num_ties; itie++) {
              /* read each tie */
              struct mbna_tie *tie = &crossing->ties[itie];
              tie->icrossing = icrossing;
              tie->itie = itie;
              if (status == MB_SUCCESS && version_id >= 302) {
                if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                    (nscan = sscanf(buffer, "TIE %d %d %d %lf %d %lf %lf %lf %lf %d %lf %lf %lf", &idummy,
                                    &tie->status, &tie->snav_1, &tie->snav_1_time_d, &tie->snav_2,
                                    &tie->snav_2_time_d, &tie->offset_x, &tie->offset_y, &tie->offset_z_m,
                                    &tie->inversion_status, &tie->inversion_offset_x, &tie->inversion_offset_y,
                                    &tie->inversion_offset_z_m)) != 13) {
                  status = MB_FAILURE;
                  fprintf(stderr, "read failed on tie: %s\n", buffer);
                }
              }
              else if (status == MB_SUCCESS && version_id >= 104) {
                if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                    (nscan = sscanf(buffer, "TIE %d %d %lf %d %lf %lf %lf %lf %d %lf %lf %lf", &idummy,
                                    &tie->snav_1, &tie->snav_1_time_d, &tie->snav_2, &tie->snav_2_time_d,
                                    &tie->offset_x, &tie->offset_y, &tie->offset_z_m, &tie->inversion_status,
                                    &tie->inversion_offset_x, &tie->inversion_offset_y,
                                    &tie->inversion_offset_z_m)) != 12) {
                  status = MB_FAILURE;
                  fprintf(stderr, "read failed on tie: %s\n", buffer);
                }
                tie->status = MBNA_TIE_XYZ;
              }
              else if (status == MB_SUCCESS) {
                if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                    (nscan = sscanf(buffer, "TIE %d %d %lf %d %lf %lf %lf %d %lf %lf", &idummy, &tie->snav_1,
                                    &tie->snav_1_time_d, &tie->snav_2, &tie->snav_2_time_d, &tie->offset_x,
                                    &tie->offset_y, &tie->inversion_status, &tie->inversion_offset_x,
                                    &tie->inversion_offset_y)) != 10) {
                  status = MB_FAILURE;
                  fprintf(stderr, "read failed on tie: %s\n", buffer);
                }
                tie->status = MBNA_TIE_XYZ;
                tie->offset_z_m = 0.0;
                tie->inversion_offset_z_m = 0.0;
              }

              /* check for outrageous inversion_offset values */
              if (fabs(tie->inversion_offset_x) > 10000.0 || fabs(tie->inversion_offset_y) > 10000.0 ||
                  fabs(tie->inversion_offset_x_m) > 10000.0 || fabs(tie->inversion_offset_y_m) > 10000.0 ||
                  fabs(tie->inversion_offset_z_m) > 10000.0) {
                tie->inversion_status = MBNA_INVERSION_OLD;
                tie->inversion_offset_x = 0.0;
                tie->inversion_offset_y = 0.0;
                tie->inversion_offset_x_m = 0.0;
                tie->inversion_offset_y_m = 0.0;
                tie->inversion_offset_z_m = 0.0;
              }

              /* reorder crossing to be early file first older file second if
                      file version prior to 3.00 */
              if (version_id < 300) {
                idummy = tie->snav_1;
                double dummy = tie->snav_1_time_d;
                tie->snav_1 = tie->snav_2;
                tie->snav_1_time_d = tie->snav_2_time_d;
                tie->snav_2 = idummy;
                tie->snav_2_time_d = dummy;
                /*                          tie->offset_x *= -1.0;
                                    tie->offset_y *= -1.0;
                                    tie->offset_z_m *= -1.0;
                                    tie->inversion_offset_x *= -1.0;
                                    tie->inversion_offset_y *= -1.0;
                                    tie->inversion_offset_z_m *= -1.0;*/
              }

              /* for version 2.0 or later read covariance */
              if (status == MB_SUCCESS && version_id >= 200) {
                if ((result = fgets(buffer, BUFFER_MAX, hfp)) != buffer ||
                    (nscan = sscanf(buffer, "COV %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &tie->sigmar1,
                                    &(tie->sigmax1[0]), &(tie->sigmax1[1]), &(tie->sigmax1[2]), &tie->sigmar2,
                                    &(tie->sigmax2[0]), &(tie->sigmax2[1]), &(tie->sigmax2[2]), &tie->sigmar3,
                                    &(tie->sigmax3[0]), &(tie->sigmax3[1]), &(tie->sigmax3[2]))) != 12) {
                  status = MB_FAILURE;
                  fprintf(stderr, "read failed on tie covariance: %s\n", buffer);
                }
                if (tie->sigmar1 <= MBNA_SMALL) {
                  tie->sigmar3 = MBNA_SMALL;
                }
                if (tie->sigmar2 <= MBNA_SMALL) {
                  tie->sigmar3 = MBNA_SMALL;
                }
                if (tie->sigmar3 <= MBNA_ZSMALL) {
                  tie->sigmar3 = MBNA_ZSMALL;
                }
              }
              else if (status == MB_SUCCESS) {
                tie->sigmar1 = 100.0;
                tie->sigmax1[0] = 1.0;
                tie->sigmax1[1] = 0.0;
                tie->sigmax1[2] = 0.0;
                tie->sigmar2 = 100.0;
                tie->sigmax2[0] = 0.0;
                tie->sigmax2[1] = 1.0;
                tie->sigmax2[2] = 0.0;
                tie->sigmar3 = 100.0;
                tie->sigmax3[0] = 0.0;
                tie->sigmax3[1] = 0.0;
                tie->sigmax3[2] = 1.0;
              }

              /* update number of ties */
              if (status == MB_SUCCESS) {
                project->num_ties++;
              }

              /* check for reasonable snav id's */
              if (status == MB_SUCCESS) {
                struct mbna_file *file = &project->files[crossing->file_id_1];
                section = &file->sections[crossing->section_1];
                if (tie->snav_1 >= section->num_snav) {
                  fprintf(stderr, "Crossing %4d:%4d %4d:%4d Reset tie snav_1 on read from %d to ",
                          crossing->file_id_1, crossing->section_1, crossing->file_id_2, crossing->section_2,
                          tie->snav_1);
                  tie->snav_1 = ((double)tie->snav_1 / (double)section->num_pings) * (MBNA_SNAV_NUM - 1);
                  tie->snav_1_time_d = section->snav_time_d[tie->snav_1];
                  fprintf(stderr, "%d because numsnav=%d\n", tie->snav_1, section->num_snav);
                }
                file = &project->files[crossing->file_id_2];
                section = &file->sections[crossing->section_2];
                if (tie->snav_2 >= section->num_snav) {
                  fprintf(stderr, "Crossing  %4d:%4d %4d:%4d  Reset tie snav_2 on read from %d to ",
                          crossing->file_id_1, crossing->section_1, crossing->file_id_2, crossing->section_2,
                          tie->snav_2);
                  tie->snav_2 = ((double)tie->snav_2 / (double)section->num_pings) * (MBNA_SNAV_NUM - 1);
                  tie->snav_2_time_d = section->snav_time_d[tie->snav_2];
                  fprintf(stderr, "%d because numsnav=%d\n", tie->snav_2, section->num_snav);
                }
              }

              /* update number of ties for snavs */
              if (status == MB_SUCCESS) {
                struct mbna_file *file = &project->files[crossing->file_id_1];
                section = &file->sections[crossing->section_1];
                section->snav_num_ties[tie->snav_1]++;
                file = &project->files[crossing->file_id_2];
                section = &file->sections[crossing->section_2];
                section->snav_num_ties[tie->snav_2]++;
              }

              /* calculate offsets in local meters */
              if (status == MB_SUCCESS) {
                tie->offset_x_m = tie->offset_x / project->mtodeglon;
                tie->offset_y_m = tie->offset_y / project->mtodeglat;
                                tie->inversion_offset_x_m = tie->inversion_offset_x / project->mtodeglon;
                                tie->inversion_offset_y_m = tie->inversion_offset_y / project->mtodeglat;
                                tie->dx_m = tie->offset_x_m - tie->inversion_offset_x_m;
                                tie->dy_m = tie->offset_y_m - tie->inversion_offset_y_m;
                                tie->dz_m = tie->offset_z_m - tie->inversion_offset_z_m;
                                tie->sigma_m = sqrt(tie->dx_m * tie->dx_m + tie->dy_m * tie->dy_m + tie->dz_m * tie->dz_m);
                                tie->dr1_m = fabs((tie->inversion_offset_x_m - tie->offset_x_m) * tie->sigmax1[0] +
                                               (tie->inversion_offset_y_m - tie->offset_y_m) * tie->sigmax1[1] +
                                               (tie->inversion_offset_z_m - tie->offset_z_m) * tie->sigmax1[2]) /
                                          tie->sigmar1;
                                tie->dr2_m = fabs((tie->inversion_offset_x_m - tie->offset_x_m) * tie->sigmax2[0] +
                                               (tie->inversion_offset_y_m - tie->offset_y_m) * tie->sigmax2[1] +
                                               (tie->inversion_offset_z_m - tie->offset_z_m) * tie->sigmax2[2]) /
                                          tie->sigmar2;
                                tie->dr3_m = fabs((tie->inversion_offset_x_m - tie->offset_x_m) * tie->sigmax3[0] +
                                               (tie->inversion_offset_y_m - tie->offset_y_m) * tie->sigmax3[1] +
                                               (tie->inversion_offset_z_m - tie->offset_z_m) * tie->sigmax3[2]) /
                                          tie->sigmar3;
                                tie->rsigma_m = sqrt(tie->dr1_m * tie->dr1_m + tie->dr2_m * tie->dr2_m + tie->dr3_m * tie->dr3_m);
              }
            }
          }

          /* finally make sure crossing has later section second, switch if needed */
          s1id = crossing->file_id_1 * 1000 + crossing->section_1;
          s2id = crossing->file_id_2 * 1000 + crossing->section_2;
          if (s2id < s1id) {
            idummy = crossing->file_id_1;
            int jdummy = crossing->section_1;
            crossing->file_id_1 = crossing->file_id_2;
            crossing->section_1 = crossing->section_2;
            crossing->file_id_2 = idummy;
            crossing->section_2 = jdummy;
            for (int itie = 0; itie < crossing->num_ties; itie++) {
              struct mbna_tie *tie = &crossing->ties[itie];
              idummy = tie->snav_1;
              double dummy = tie->snav_1_time_d;
              tie->snav_1 = tie->snav_2;
              tie->snav_1_time_d = tie->snav_2_time_d;
              tie->snav_2 = idummy;
              tie->snav_2_time_d = dummy;
              tie->offset_x *= -1.0;
              tie->offset_y *= -1.0;
              tie->offset_x_m *= -1.0;
              tie->offset_y_m *= -1.0;
              tie->offset_z_m *= -1.0;
              tie->inversion_offset_x *= -1.0;
              tie->inversion_offset_y *= -1.0;
              tie->inversion_offset_x_m *= -1.0;
              tie->inversion_offset_y_m *= -1.0;
              tie->inversion_offset_z_m *= -1.0;
                            tie->dx_m *= -1.0;
                            tie->dy_m *= -1.0;
                            tie->dz_m *= -1.0;
                            //tie->sigma_m;
                            tie->dr1_m *= -1.0;
                            tie->dr2_m *= -1.0;
                            tie->dr3_m *= -1.0;
                            //tie->rsigma_m;
            }
          }

          /* and even more finally, reset the snav times for the ties */
          for (int itie = 0; itie < crossing->num_ties; itie++) {
            struct mbna_tie *tie = &crossing->ties[itie];
            section1 = &(project->files[crossing->file_id_1].sections[crossing->section_1]);
            section2 = &(project->files[crossing->file_id_2].sections[crossing->section_2]);
            tie->snav_1_time_d = section1->snav_time_d[tie->snav_1];
            tie->snav_2_time_d = section2->snav_time_d[tie->snav_2];
          }
        }

        /* close home file */
        fclose(hfp);

        /* set project status flag */
        if (status == MB_SUCCESS)
          project->open = true;
        else {
          for (int ifile = 0; ifile < project->num_files; ifile++) {
            struct mbna_file *file = &project->files[ifile];
            if (file->sections != NULL)
              free(file->sections);
          }
          if (project->files != NULL)
            free(project->files);
          if (project->crossings != NULL)
            free(project->crossings);
          project->open = false;
          memset(project->name, 0, STRING_MAX);
          strcpy(project->name, "None");
          memset(project->path, 0, STRING_MAX);
          memset(project->datadir, 0, STRING_MAX);
          project->num_files = 0;
          project->num_files_alloc = 0;
          project->num_snavs = 0;
          project->num_pings = 0;
          project->num_beams = 0;
          project->num_crossings = 0;
          project->num_crossings_alloc = 0;
          project->num_crossings_analyzed = 0;
          project->num_goodcrossings = 0;
          project->num_truecrossings = 0;
          project->num_truecrossings_analyzed = 0;
          project->num_ties = 0;
        }

        /* recalculate crossing overlap values if not already set */
        if (project->open) {
          for (int icrossing = 0; icrossing < project->num_crossings; icrossing++) {
            struct mbna_crossing *crossing = &(project->crossings[icrossing]);
            if (crossing->overlap <= 0) {
              mbnavadjust_crossing_overlap(verbose, project, icrossing, error);
            }
            if (crossing->overlap >= 25)
              project->num_goodcrossings++;
          }
        }
      }

      /* else set error */
      else {
        status = MB_FAILURE;
      }
    }

    /* open log file */
    if ((project->logfp = fopen(project->logfile, "a")) != NULL) {
      fprintf(project->logfp,
              "Project opened: %s\n > Project home: %s\n > Number of Files: %d\n > Number of Crossings Found: %d\n > "
              "Number of Crossings Analyzed: %d\n > Number of Navigation Ties: %d\n",
              project->name, project->home, project->num_files, project->num_crossings, project->num_crossings_analyzed,
              project->num_ties);
    }
    else {
      fprintf(stderr, "Failure to open log file %s\n", project->logfile);
      *error = MB_ERROR_INIT_FAIL;
      status = MB_FAILURE;
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_close_project(int verbose, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:            %d\n", verbose);
    fprintf(stderr, "dbg2       project:            %p\n", project);
  }

  /* add info text */
  fprintf(project->logfp, "Project closed: %s\n", project->name);
  fprintf(project->logfp, "Log file %s/log.txt closed\n", project->datadir);

  int status = MB_SUCCESS;
  struct mbna_file *file;

  /* deallocate memory and reset values */
  for (int i = 0; i < project->num_files; i++) {
    file = &project->files[i];
    if (file->sections != NULL)
      mb_freed(verbose, __FILE__, __LINE__, (void **)&file->sections, error);
  }
  if (project->files != NULL) {
    free(project->files);
    project->files = NULL;
    project->num_files_alloc = 0;
  }
  if (project->crossings != NULL) {
    free(project->crossings);
    project->crossings = NULL;
    project->num_crossings_alloc = 0;
  }
  if (project->logfp != NULL) {
    fclose(project->logfp);
    project->logfp = NULL;
  }

  /* reset values */
  project->open = false;
  memset(project->name, 0, STRING_MAX);
  strcpy(project->name, "None");
  memset(project->path, 0, STRING_MAX);
  memset(project->datadir, 0, STRING_MAX);
  memset(project->logfile, 0, STRING_MAX);
  project->num_files = 0;
  project->num_snavs = 0;
  project->num_pings = 0;
  project->num_beams = 0;
  project->num_crossings = 0;
  project->num_crossings_analyzed = 0;
  project->num_goodcrossings = 0;
  project->num_truecrossings = 0;
  project->num_truecrossings_analyzed = 0;
  project->num_ties = 0;
  project->inversion_status = MBNA_INVERSION_NONE;
  project->grid_status = MBNA_GRID_NONE;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_write_project(int verbose, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:            %d\n", verbose);
    fprintf(stderr, "dbg2       project:            %p\n", project);
  }

  int status = MB_SUCCESS;
  FILE *hfp;
  struct mbna_file *file, *file_1, *file_2;
  struct mbna_section *section, *section_1, *section_2;
  struct mbna_crossing *crossing;
  struct mbna_tie *tie;
  char datalist[STRING_MAX];
  char routefile[STRING_MAX];
  char routename[STRING_MAX];
  char offsetfile[STRING_MAX];
  double navlon1, navlon2, navlat1, navlat2;
    int time_i[7];
  int nroute;
  int snav_1, snav_2;
  int ncrossings_true = 0;
  int ncrossings_gt50 = 0;
  int ncrossings_gt25 = 0;
  int ncrossings_lt25 = 0;
  int ncrossings_fixed = 0;
  int nties_unfixed = 0;
  int nties_fixed = 0;
  char status_char, truecrossing_char;
  int routecolor = 1;
  char *unknown = "Unknown";
  double mtodeglon, mtodeglat;

  /* time, user, host variables */
  time_t right_now;
  char date[32], user[MB_PATH_MAXLINE], *user_ptr, host[MB_PATH_MAXLINE];

  int i, j, k, l;

  /* open and write home file */
  if ((hfp = fopen(project->home, "w")) != NULL) {
    fprintf(stderr, "Writing project %s (file version 3.09)\n", project->name);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "##MBNAVADJUST PROJECT\n");
    fprintf(hfp, "MB-SYSTEM_VERSION\t%s\n", MB_VERSION);
    fprintf(hfp, "PROGRAM_VERSION\t3.09\n");
    fprintf(hfp, "FILE_VERSION\t3.09\n");
    fprintf(hfp, "ORIGIN\tGenerated by user <%s> on cpu <%s> at <%s>\n", user, host, date);
    fprintf(hfp, "NAME\t%s\n", project->name);
    fprintf(hfp, "PATH\t%s\n", project->path);
    fprintf(hfp, "HOME\t%s\n", project->home);
    fprintf(hfp, "DATADIR\t%s\n", project->datadir);
    fprintf(hfp, "NUMFILES\t%d\n", project->num_files);
    fprintf(hfp, "NUMBLOCKS\t%d\n", project->num_surveys);
    fprintf(hfp, "NUMCROSSINGS\t%d\n", project->num_crossings);
    fprintf(hfp, "SECTIONLENGTH\t%f\n", project->section_length);
    fprintf(hfp, "SECTIONSOUNDINGS\t%d\n", project->section_soundings);
    fprintf(hfp, "DECIMATION\t%d\n", project->decimation);
    fprintf(hfp, "CONTOURINTERVAL\t%f\n", project->cont_int);
    fprintf(hfp, "COLORINTERVAL\t%f\n", project->col_int);
    fprintf(hfp, "TICKINTERVAL\t%f\n", project->tick_int);
    fprintf(hfp, "INVERSION\t%d\n", project->inversion_status);
    fprintf(hfp, "GRIDSTATUS\t%d\n", project->grid_status);
    fprintf(hfp, "SMOOTHING\t%f\n", project->smoothing);
    fprintf(hfp, "ZOFFSETWIDTH\t%f\n", project->zoffsetwidth);
    for (i = 0; i < project->num_files; i++) {
      /* write out basic file info */
      file = &project->files[i];
      fprintf(hfp, "FILE %4d %4d %4d %4d %4d %13.8f %13.8f %13.8f %4.1f %4.1f %4.1f %4.1f %4d %4d %s\n", i, file->status,
              file->id, file->format, file->block, file->block_offset_x, file->block_offset_y, file->block_offset_z,
              file->heading_bias_import, file->roll_bias_import, file->heading_bias, file->roll_bias, file->num_sections,
              file->output_id, file->file);

      /* write out section info */
      for (int j = 0; j < file->num_sections; j++) {
        section = &file->sections[j];
        fprintf(hfp, "SECTION %4d %5d %5d %d %d %10.6f %16.6f %16.6f %13.8f %13.8f %13.8f %13.8f %9.3f %9.3f %d\n", j,
                section->num_pings, section->num_beams, section->num_snav, section->continuity, section->distance,
                section->btime_d, section->etime_d, section->lonmin, section->lonmax, section->latmin, section->latmax,
                section->depthmin, section->depthmax, section->contoursuptodate);
        for (k = MBNA_MASK_DIM - 1; k >= 0; k--) {
          for (l = 0; l < MBNA_MASK_DIM; l++) {
            fprintf(hfp, "%1d", section->coverage[l + k * MBNA_MASK_DIM]);
          }
          fprintf(hfp, "\n");
        }
        for (k = 0; k < section->num_snav; k++) {
          fprintf(hfp, "SNAV %4d %5d %10.6f %16.6f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f\n",
                            k, section->snav_id[k], section->snav_distance[k], section->snav_time_d[k],
                            section->snav_lon[k], section->snav_lat[k], section->snav_sensordepth[k],
                  section->snav_lon_offset[k], section->snav_lat_offset[k], section->snav_z_offset[k]);
        }
        if (section->global_tie_status == 0 && section->global_tie_snav == -1) {
          section->global_tie_inversion_status = 0;
          section->offset_x = 0.0;
          section->offset_y = 0.0;
          section->offset_z_m = 0.0;
          section->xsigma = 0.0;
          section->ysigma = 0.0;
          section->zsigma = 0.0;
        }
        fprintf(hfp, "GLOBALTIE %2d %4d %2d %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f\n",
                        section->global_tie_status, section->global_tie_snav, section->global_tie_inversion_status,
                        section->offset_x, section->offset_y, section->offset_z_m,
                        section->xsigma, section->ysigma, section->zsigma);
      }
    }

    /* write out crossing info */
    for (i = 0; i < project->num_crossings; i++) {
      /* write out basic crossing info */
      crossing = &project->crossings[i];
      fprintf(hfp, "CROSSING %5d %d %d %3d %5d %3d %5d %3d %2d\n", i, crossing->status, crossing->truecrossing,
              crossing->overlap, crossing->file_id_1, crossing->section_1, crossing->file_id_2, crossing->section_2,
              crossing->num_ties);

      /* write out tie info */
      for (int j = 0; j < crossing->num_ties; j++) {
        /* write out basic tie info */
        struct mbna_tie *tie = &crossing->ties[j];
        fprintf(hfp, "TIE %5d %1d %5d %16.6f %5d %16.6f %13.8f %13.8f %13.8f %1.1d %13.8f %13.8f %13.8f\n", j,
                tie->status, tie->snav_1, tie->snav_1_time_d, tie->snav_2, tie->snav_2_time_d, tie->offset_x,
                tie->offset_y, tie->offset_z_m, tie->inversion_status, tie->inversion_offset_x, tie->inversion_offset_y,
                tie->inversion_offset_z_m);
        fprintf(hfp, "COV %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f %13.8f\n",
                tie->sigmar1, tie->sigmax1[0], tie->sigmax1[1], tie->sigmax1[2], tie->sigmar2, tie->sigmax2[0],
                tie->sigmax2[1], tie->sigmax2[2], tie->sigmar3, tie->sigmax3[0], tie->sigmax3[1], tie->sigmax3[2]);
      }
    }

    /* close home file */
    fclose(hfp);
    status = MB_SUCCESS;
  }

  /* else set error */
  else {
    status = MB_FAILURE;
    *error = MB_ERROR_WRITE_FAIL;
    fprintf(stderr, "Unable to update project %s\n > Home file: %s\n", project->name, project->home);
  }

  /* open and write datalist file */
  sprintf(datalist, "%s%s.mb-1", project->path, project->name);
  if ((hfp = fopen(datalist, "w")) != NULL) {
    for (i = 0; i < project->num_files; i++) {
      /* write file entry */
      file = &project->files[i];
      fprintf(hfp, "%s %d\n", file->file, file->format);
    }
    fclose(hfp);
  }

  /* else set error */
  else {
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, "Unable to update project %s\n > Datalist file: %s\n", project->name, datalist);
  }

  /* write mbgrdviz route files in which each tie point or crossing is a two point route
      consisting of the connected snav points
      - output several different route files
      - route files of ties (fixed and unfixed separate) represent each tie as a
          two point route consisting of the connected snav points
      - route files of crossings (<25%, >= 25% && < 50%, >= 50%, true crossings)
          represent each crossing as a two point route consisting of the central
          snav points for each of the two sections.
      - first count different types of crossings and ties to output as routes
      - then output each time of route file */
  ncrossings_true = 0;
  ncrossings_gt50 = 0;
  ncrossings_gt25 = 0;
  ncrossings_lt25 = 0;
  ncrossings_fixed = 0;
  nties_unfixed = 0;
  nties_fixed = 0;
  for (i = 0; i < project->num_crossings; i++) {
    crossing = &project->crossings[i];

    /* check all crossings */
    if (project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
        project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)
      ncrossings_fixed++;
    else {
      if (crossing->truecrossing)
        ncrossings_true++;
      else if (crossing->overlap >= 50)
        ncrossings_gt50++;
      else if (crossing->overlap >= 25)
        ncrossings_gt25++;
      else
        ncrossings_lt25++;
    }

    /* check ties */
    if (crossing->status == MBNA_CROSSING_STATUS_SET) {
      if (project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
          project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)
        nties_fixed += crossing->num_ties;
      else
        nties_unfixed += crossing->num_ties;
    }
  }

  /* write mbgrdviz route file for all unfixed true crossings */
  sprintf(routefile, "%s%s_truecrossing.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output tie route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", ncrossings_true);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only unfixed true crossings */
      if (crossing->truecrossing && !(project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
                                                project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
        file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
        section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
        section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
        snav_1 = section_1->num_snav / 2;
        snav_2 = section_2->num_snav / 2;
        navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
        navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
        navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
        navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
        if (crossing->status == MBNA_CROSSING_STATUS_NONE) {
          status_char = 'U';
          routecolor = ROUTE_COLOR_YELLOW;
        }
        else if (crossing->status == MBNA_CROSSING_STATUS_SET) {
          status_char = '*';
          routecolor = ROUTE_COLOR_GREEN;
        }
        else {
          status_char = '-';
          routecolor = ROUTE_COLOR_RED;
        }
        if (!crossing->truecrossing)
          truecrossing_char = ' ';
        else
          truecrossing_char = 'X';
        sprintf(routename, "%c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d", status_char, truecrossing_char, i,
                file_1->block, crossing->file_id_1, crossing->section_1, file_2->block, crossing->file_id_2,
                crossing->section_2, crossing->overlap, crossing->num_ties);
        fprintf(hfp, "## ROUTENAME %s\n", routename);
        fprintf(hfp, "## ROUTESIZE %d\n", 1);
        fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
        fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
        fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
        fprintf(hfp, "> ## STARTROUTE\n");
        fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
        nroute++;
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) true crossing locations to %s\n", nroute, ncrossings_true, routefile);
  }

  /* write mbgrdviz route file for all unfixed >=50% crossings */
  sprintf(routefile, "%s%s_gt50crossing.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output tie route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", ncrossings_gt50);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only unfixed >=50% crossings */
      if (crossing->overlap >= 50 && !(project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
                                       project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
        file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
        section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
        section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
        snav_1 = section_1->num_snav / 2;
        snav_2 = section_2->num_snav / 2;
        navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
        navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
        navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
        navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
        if (crossing->status == MBNA_CROSSING_STATUS_NONE) {
          status_char = 'U';
          routecolor = ROUTE_COLOR_YELLOW;
        }
        else if (crossing->status == MBNA_CROSSING_STATUS_SET) {
          status_char = '*';
          routecolor = ROUTE_COLOR_GREEN;
        }
        else {
          status_char = '-';
          routecolor = ROUTE_COLOR_RED;
        }
        if (!crossing->truecrossing)
          truecrossing_char = ' ';
        else
          truecrossing_char = 'X';
        sprintf(routename, "%c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d", status_char, truecrossing_char, i,
                file_1->block, crossing->file_id_1, crossing->section_1, file_2->block, crossing->file_id_2,
                crossing->section_2, crossing->overlap, crossing->num_ties);
        fprintf(hfp, "## ROUTENAME %s\n", routename);
        fprintf(hfp, "## ROUTESIZE %d\n", 1);
        fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
        fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
        fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
        fprintf(hfp, "> ## STARTROUTE\n");
        fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
        nroute++;
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) >=50%% overlap crossing locations to %s\n", nroute, ncrossings_gt50,
    // routefile);
  }

  /* write mbgrdviz route file for all unfixed >=25% but less than 50% crossings */
  sprintf(routefile, "%s%s_gt25crossing.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output tie route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", ncrossings_gt25);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only unfixed >=25% but less than 50% crossings crossings */
      if (crossing->overlap >= 25 && !(project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
                                       project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
        file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
        section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
        section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
        snav_1 = section_1->num_snav / 2;
        snav_2 = section_2->num_snav / 2;
        navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
        navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
        navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
        navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
        if (crossing->status == MBNA_CROSSING_STATUS_NONE) {
          status_char = 'U';
          routecolor = ROUTE_COLOR_YELLOW;
        }
        else if (crossing->status == MBNA_CROSSING_STATUS_SET) {
          status_char = '*';
          routecolor = ROUTE_COLOR_GREEN;
        }
        else {
          status_char = '-';
          routecolor = ROUTE_COLOR_RED;
        }
        if (!crossing->truecrossing)
          truecrossing_char = ' ';
        else
          truecrossing_char = 'X';
        sprintf(routename, "%c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d", status_char, truecrossing_char, i,
                file_1->block, crossing->file_id_1, crossing->section_1, file_2->block, crossing->file_id_2,
                crossing->section_2, crossing->overlap, crossing->num_ties);
        fprintf(hfp, "## ROUTENAME %s\n", routename);
        fprintf(hfp, "## ROUTESIZE %d\n", 1);
        fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
        fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
        fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
        fprintf(hfp, "> ## STARTROUTE\n");
        fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
        nroute++;
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) >=25%% && < 50%% overlap crossing locations to %s\n", nroute, ncrossings_gt25,
    // routefile);
  }

  /* write mbgrdviz route file for all unfixed <25% crossings */
  sprintf(routefile, "%s%s_lt25crossing.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output tie route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", ncrossings_lt25);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only unfixed <25% crossings crossings */
      if (crossing->overlap < 25 && !(project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
                                      project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
        file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
        section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
        section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
        snav_1 = section_1->num_snav / 2;
        snav_2 = section_2->num_snav / 2;
        navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
        navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
        navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
        navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
        if (crossing->status == MBNA_CROSSING_STATUS_NONE) {
          status_char = 'U';
          routecolor = ROUTE_COLOR_YELLOW;
        }
        else if (crossing->status == MBNA_CROSSING_STATUS_SET) {
          status_char = '*';
          routecolor = ROUTE_COLOR_GREEN;
        }
        else {
          status_char = '-';
          routecolor = ROUTE_COLOR_RED;
        }
        if (!crossing->truecrossing)
          truecrossing_char = ' ';
        else
          truecrossing_char = 'X';
        sprintf(routename, "%c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d", status_char, truecrossing_char, i,
                file_1->block, crossing->file_id_1, crossing->section_1, file_2->block, crossing->file_id_2,
                crossing->section_2, crossing->overlap, crossing->num_ties);
        fprintf(hfp, "## ROUTENAME %s\n", routename);
        fprintf(hfp, "## ROUTESIZE %d\n", 1);
        fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
        fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
        fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
        fprintf(hfp, "> ## STARTROUTE\n");
        fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
        nroute++;
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) <25%% overlap crossing locations to %s\n", nroute, ncrossings_lt25, routefile);
  }

  /* write mbgrdviz route file for all fixed crossings */
  sprintf(routefile, "%s%s_fixedcrossing.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output fixed crossings route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", ncrossings_fixed);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only fixed crossings */
      if (project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
          project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV) {
        file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
        file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
        section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
        section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
        snav_1 = section_1->num_snav / 2;
        snav_2 = section_2->num_snav / 2;
        navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
        navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
        navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
        navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
        if (crossing->status == MBNA_CROSSING_STATUS_NONE) {
          status_char = 'U';
          routecolor = ROUTE_COLOR_YELLOW;
        }
        else if (crossing->status == MBNA_CROSSING_STATUS_SET) {
          status_char = '*';
          routecolor = ROUTE_COLOR_GREEN;
        }
        else {
          status_char = '-';
          routecolor = ROUTE_COLOR_RED;
        }
        if (!crossing->truecrossing)
          truecrossing_char = ' ';
        else
          truecrossing_char = 'X';
        sprintf(routename, "%c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d", status_char, truecrossing_char, i,
                file_1->block, crossing->file_id_1, crossing->section_1, file_2->block, crossing->file_id_2,
                crossing->section_2, crossing->overlap, crossing->num_ties);
        fprintf(hfp, "## ROUTENAME %s\n", routename);
        fprintf(hfp, "## ROUTESIZE %d\n", 1);
        fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
        fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
        fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
        fprintf(hfp, "> ## STARTROUTE\n");
        fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
        nroute++;
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) fixed crossing locations to %s\n", nroute, ncrossings_fixed, routefile);
  }

  /* write mbgrdviz route file for all unfixed ties */
  sprintf(routefile, "%s%s_unfixedties.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output unfixed ties route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", nties_unfixed);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");
    routecolor = ROUTE_COLOR_BLUEGREEN;

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only unfixed ties */
      if (crossing->status == MBNA_CROSSING_STATUS_SET &&
          !(project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
            project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        for (j = 0; j < crossing->num_ties; j++) {
          file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
          file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
          section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
          section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
          tie = (struct mbna_tie *)&crossing->ties[j];
          snav_1 = tie->snav_1;
          snav_2 = tie->snav_2;
          navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
          navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
          navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
          navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
          if (crossing->status == MBNA_CROSSING_STATUS_NONE)
            status_char = 'U';
          else if (crossing->status == MBNA_CROSSING_STATUS_SET)
            status_char = '*';
          else
            status_char = '-';
          if (!crossing->truecrossing)
            truecrossing_char = ' ';
          else
            truecrossing_char = 'X';
          sprintf(routename, "Tie: %c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d of %2d", status_char,
                  truecrossing_char, i, file_1->block, crossing->file_id_1, crossing->section_1, file_2->block,
                  crossing->file_id_2, crossing->section_2, crossing->overlap, j, crossing->num_ties);
          fprintf(hfp, "## ROUTENAME %s\n", routename);
          fprintf(hfp, "## ROUTESIZE %d\n", 1);
          fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
          fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
          fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
          fprintf(hfp, "> ## STARTROUTE\n");
          fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
          nroute++;
        }
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) unfixed tie locations to %s\n", nroute, nties_unfixed, routefile);
  }

  /* write mbgrdviz route file for all fixed ties */
  sprintf(routefile, "%s%s_fixedties.rte", project->path, project->name);
  if ((hfp = fopen(routefile, "w")) == NULL) {
    fclose(hfp);
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
    fprintf(stderr, " > Unable to open output fixed ties route file %s\n", routefile);
  }
  else {
    /* write the route file header */
    fprintf(hfp, "## Route File Version %s\n", ROUTE_VERSION);
    fprintf(hfp, "## Output by Program %s\n", program_name);
    fprintf(hfp, "## MB-System Version %s\n", MB_VERSION);
    char user[256], host[256], date[32];
    status = mb_user_host_date(verbose, user, host, date, error);
    fprintf(hfp, "## Run by user <%s> on cpu <%s> at <%s>\n", user_ptr, host, date);
    fprintf(hfp, "## Number of routes: %d\n", nties_fixed);
    fprintf(hfp, "## Route point format:\n");
    fprintf(hfp, "##   <longitude (deg)> <latitude (deg)> <topography (m)> <waypoint (boolean)>\n");
    routecolor = ROUTE_COLOR_RED;

    /* loop over all crossings */
    nroute = 0;
    for (i = 0; i < project->num_crossings; i++) {
      crossing = &project->crossings[i];

      /* output only fixed ties */
      if (crossing->status == MBNA_CROSSING_STATUS_SET &&
          (project->files[crossing->file_id_1].status == MBNA_FILE_FIXEDNAV ||
           project->files[crossing->file_id_2].status == MBNA_FILE_FIXEDNAV)) {
        for (j = 0; j < crossing->num_ties; j++) {
          file_1 = (struct mbna_file *)&project->files[crossing->file_id_1];
          file_2 = (struct mbna_file *)&project->files[crossing->file_id_2];
          section_1 = (struct mbna_section *)&file_1->sections[crossing->section_1];
          section_2 = (struct mbna_section *)&file_2->sections[crossing->section_2];
          tie = (struct mbna_tie *)&crossing->ties[j];
          snav_1 = tie->snav_1;
          snav_2 = tie->snav_2;
          navlon1 = section_1->snav_lon[snav_1] + section_1->snav_lon_offset[snav_1];
          navlat1 = section_1->snav_lat[snav_1] + section_1->snav_lat_offset[snav_1];
          navlon2 = section_2->snav_lon[snav_2] + section_2->snav_lon_offset[snav_2];
          navlat2 = section_2->snav_lat[snav_2] + section_2->snav_lat_offset[snav_2];
          if (crossing->status == MBNA_CROSSING_STATUS_NONE)
            status_char = 'U';
          else if (crossing->status == MBNA_CROSSING_STATUS_SET)
            status_char = '*';
          else
            status_char = '-';
          if (!crossing->truecrossing)
            truecrossing_char = ' ';
          else
            truecrossing_char = 'X';
          sprintf(routename, "Tie: %c%c %4d %2.2d:%3.3d:%3.3d %2.2d:%3.3d:%3.3d %3d %2d of %2d", status_char,
                  truecrossing_char, i, file_1->block, crossing->file_id_1, crossing->section_1, file_2->block,
                  crossing->file_id_2, crossing->section_2, crossing->overlap, j, crossing->num_ties);
          fprintf(hfp, "## ROUTENAME %s\n", routename);
          fprintf(hfp, "## ROUTESIZE %d\n", 1);
          fprintf(hfp, "## ROUTECOLOR %d\n", routecolor);
          fprintf(hfp, "## ROUTEPOINTS %d\n", 2);
          fprintf(hfp, "## ROUTEEDITMODE %d\n", false);
          fprintf(hfp, "> ## STARTROUTE\n");
          fprintf(hfp, "%.10f %.10f 0.00 1\n%.10f %.10f 0.00 1\n>\n", navlon1, navlat1, navlon2, navlat2);
          nroute++;
        }
      }
    }
    fclose(hfp);
    // fprintf(stderr,"Output %d (expected %d) fixed tie locations to %s\n", nroute, nties_fixed, routefile);
  }

  /* output offset vectors */
  if (project->inversion_status == MBNA_INVERSION_CURRENT) {
    sprintf(offsetfile, "%s%s_offset.txt", project->path, project->name);
    if ((hfp = fopen(offsetfile, "w")) != NULL) {
      for (i = 0; i < project->num_files; i++) {
        file = &project->files[i];
        for (j = 0; j < file->num_sections; j++) {
          section = &file->sections[j];
          mb_coor_scale(verbose, 0.5 * (section->latmin + section->latmax), &mtodeglon, &mtodeglat);
          for (k = 0; k < section->num_snav; k++) {
                        mb_get_date(verbose, section->snav_time_d[k], time_i);
                        fprintf(hfp, "%4.4d:%4.4d:%2.2d  %4d/%2d/%2d %2d:%2d:%2d.%6.6d  %.6f %8.3f %8.3f %6.3f\n",
                                        i, j, k, time_i[0],time_i[1],time_i[2],time_i[3],time_i[4],time_i[5],time_i[6],
                                        section->snav_time_d[k],
                                        (section->snav_lon_offset[k] / mtodeglon),
                                        (section->snav_lat_offset[k] / mtodeglat),
                                        section->snav_z_offset[k]);
          }
        }
      }
      fclose(hfp);
    } else {
      status = MB_FAILURE;
      *error = MB_ERROR_OPEN_FAIL;
      fprintf(stderr, "Unable to update project %s\n > Offset file: %s\n", project->name, offsetfile);
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_crossing_overlap(int verbose, struct mbna_project *project, int crossing_id, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:              %d\n", verbose);
    fprintf(stderr, "dbg2       project:              %p\n", project);
    fprintf(stderr, "dbg2       crossing_id:          %d\n", crossing_id);
  }

  struct mbna_crossing *crossing =
      (struct mbna_crossing *)&project->crossings[crossing_id];

  /* get section endpoints */
  struct mbna_file *file = &project->files[crossing->file_id_1];
  struct mbna_section *section1 = &file->sections[crossing->section_1];
  file = &project->files[crossing->file_id_2];
  struct mbna_section *section2 = &file->sections[crossing->section_2];
  const double lonoffset = section2->snav_lon_offset[section2->num_snav / 2] - section1->snav_lon_offset[section1->num_snav / 2];
  const double latoffset = section2->snav_lat_offset[section2->num_snav / 2] - section1->snav_lat_offset[section1->num_snav / 2];

  /* initialize overlap arrays */
  int overlap1[MBNA_MASK_DIM * MBNA_MASK_DIM];
  int overlap2[MBNA_MASK_DIM * MBNA_MASK_DIM];
  for (int i = 0; i < MBNA_MASK_DIM * MBNA_MASK_DIM; i++) {
    overlap1[i] = 0;
    overlap2[i] = 0;
  }

  /* check coverage masks for overlap */
  // int first = true;
  const double dx1 = (section1->lonmax - section1->lonmin) / MBNA_MASK_DIM;
  const double dy1 = (section1->latmax - section1->latmin) / MBNA_MASK_DIM;
  const double dx2 = (section2->lonmax - section2->lonmin) / MBNA_MASK_DIM;
  const double dy2 = (section2->latmax - section2->latmin) / MBNA_MASK_DIM;
  for (int ii1 = 0; ii1 < MBNA_MASK_DIM; ii1++) {
    for (int jj1 = 0; jj1 < MBNA_MASK_DIM; jj1++) {
      const int kk1 = ii1 + jj1 * MBNA_MASK_DIM;
      if (section1->coverage[kk1] == 1) {
        const double lon1min = section1->lonmin + dx1 * ii1;
        const double lon1max = section1->lonmin + dx1 * (ii1 + 1);
        const double lat1min = section1->latmin + dy1 * jj1;
        const double lat1max = section1->latmin + dy1 * (jj1 + 1);
        for (int ii2 = 0; ii2 < MBNA_MASK_DIM; ii2++)
          for (int jj2 = 0; jj2 < MBNA_MASK_DIM; jj2++) {
            const int kk2 = ii2 + jj2 * MBNA_MASK_DIM;
            if (section2->coverage[kk2] == 1) {
              const double lon2min = section2->lonmin + dx2 * ii2 + lonoffset;
              const double lon2max = section2->lonmin + dx2 * (ii2 + 1) + lonoffset;
              const double lat2min = section2->latmin + dy2 * jj2 + latoffset;
              const double lat2max = section2->latmin + dy2 * (jj2 + 1) + latoffset;
              if ((lon1min < lon2max) && (lon1max > lon2min) && (lat1min < lat2max) && (lat1max > lat2min)) {
                overlap1[kk1] = 1;
                overlap2[kk2] = 1;
              }
            }
          }
      }
    }
  }

  /* count fractions covered */
  int ncoverage1 = 0;
  int ncoverage2 = 0;
  int noverlap1 = 0;
  int noverlap2 = 0;
  for (int i = 0; i < MBNA_MASK_DIM * MBNA_MASK_DIM; i++) {
    if (section1->coverage[i] == 1)
      ncoverage1++;
    if (section2->coverage[i] == 1)
      ncoverage2++;
    if (overlap1[i] == 1)
      noverlap1++;
    if (overlap2[i] == 1)
      noverlap2++;
  }
  //const double overlapfraction =
  //    (dx1 * dy1) / (dx1 * dy1 + dx2 * dy2) * ((double)noverlap1) / ((double)ncoverage1) +
  //    (dx2 * dy2) / (dx1 * dy1 + dx2 * dy2) * ((double)noverlap2) / ((double)ncoverage2);
  const double overlapfraction = 0.5 * ((double)noverlap1) / ((double)ncoverage1)
                                  + 0.5 * ((double)noverlap2) / ((double)ncoverage2);
  crossing->overlap = (int)(100.0 * overlapfraction);
  if (crossing->overlap < 1)
    crossing->overlap = 1;

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       crossing->overlap: %d\n", crossing->overlap);
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_crossing_overlapbounds(int verbose, struct mbna_project *project, int crossing_id, double offset_x,
                                       double offset_y, double *lonmin, double *lonmax, double *latmin, double *latmax,
                                       int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:              %d\n", verbose);
    fprintf(stderr, "dbg2       project:              %p\n", project);
    fprintf(stderr, "dbg2       crossing_id:          %d\n", crossing_id);
    fprintf(stderr, "dbg2       offset_x:             %f\n", offset_x);
    fprintf(stderr, "dbg2       offset_y:             %f\n", offset_y);
  }

  struct mbna_crossing *crossing =
      (struct mbna_crossing *)&project->crossings[crossing_id];

  /* get section endpoints */
  struct mbna_file *file = &project->files[crossing->file_id_1];
  struct mbna_section *section1 = &file->sections[crossing->section_1];
  file = &project->files[crossing->file_id_2];
  struct mbna_section *section2 = &file->sections[crossing->section_2];

  // int overlap1[MBNA_MASK_DIM * MBNA_MASK_DIM];
  // int overlap2[MBNA_MASK_DIM * MBNA_MASK_DIM];

  /* initialize overlap arrays */
  // for (int i = 0; i < MBNA_MASK_DIM * MBNA_MASK_DIM; i++) {
  //   overlap1[i] = 0;
  //   overlap2[i] = 0;
  // }

  /* get overlap region bounds and focus point */
  *lonmin = 0.0;
  *lonmax = 0.0;
  *latmin = 0.0;
  *latmax = 0.0;
  const double dx1 = (section1->lonmax - section1->lonmin) / MBNA_MASK_DIM;
  const double dy1 = (section1->latmax - section1->latmin) / MBNA_MASK_DIM;
  const double dx2 = (section2->lonmax - section2->lonmin) / MBNA_MASK_DIM;
  const double dy2 = (section2->latmax - section2->latmin) / MBNA_MASK_DIM;

  int first = true;  // TODO(schwehr): bool

  for (int ii1 = 0; ii1 < MBNA_MASK_DIM; ii1++) {
    for (int jj1 = 0; jj1 < MBNA_MASK_DIM; jj1++) {
      const int kk1 = ii1 + jj1 * MBNA_MASK_DIM;
      if (section1->coverage[kk1] == 1) {
        const double lon1min = section1->lonmin + dx1 * ii1;
        const double lon1max = section1->lonmin + dx1 * (ii1 + 1);
        const double lat1min = section1->latmin + dy1 * jj1;
        const double lat1max = section1->latmin + dy1 * (jj1 + 1);
        for (int ii2 = 0; ii2 < MBNA_MASK_DIM; ii2++) {
          for (int jj2 = 0; jj2 < MBNA_MASK_DIM; jj2++) {
            const int kk2 = ii2 + jj2 * MBNA_MASK_DIM;
            if (section2->coverage[kk2] == 1) {
              const double lon2min = section2->lonmin + dx2 * ii2 + offset_x;
              const double lon2max = section2->lonmin + dx2 * (ii2 + 1) + offset_x;
              const double lat2min = section2->latmin + dy2 * jj2 + offset_y;
              const double lat2max = section2->latmin + dy2 * (jj2 + 1) + offset_y;
              if ((lon1min < lon2max) && (lon1max > lon2min) && (lat1min < lat2max) && (lat1max > lat2min)) {
                // overlap1[kk1] = 1;
                // overlap2[kk2] = 1;
                if (!first) {
                  *lonmin = MIN(*lonmin, MAX(lon1min, lon2min));
                  *lonmax = MAX(*lonmax, MIN(lon1max, lon2max));
                  *latmin = MIN(*latmin, MAX(lat1min, lat2min));
                  *latmax = MAX(*latmax, MIN(lat1max, lat2max));
                }
                else {
                  first = false;
                  *lonmin = MAX(lon1min, lon2min);
                  *lonmax = MIN(lon1max, lon2max);
                  *latmin = MAX(lat1min, lat2min);
                  *latmax = MIN(lat1max, lat2max);
                }
              }
            }
          }
                }
      }
    }
  }

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       lonmin:      %.10f\n", *lonmin);
    fprintf(stderr, "dbg2       lonmax:      %.10f\n", *lonmax);
    fprintf(stderr, "dbg2       latmin:      %.10f\n", *latmin);
    fprintf(stderr, "dbg2       latmax:      %.10f\n", *latmax);
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_crossing_focuspoint(int verbose, struct mbna_project *project, int crossing_id,
                                    double offset_x, double offset_y, int *isnav1_focus, int *isnav2_focus,
                                    double *lon_focus, double *lat_focus, int *error) {
  (void)isnav1_focus;  // Unused parameter
  (void)isnav2_focus;  // Unused parameter

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:              %d\n", verbose);
    fprintf(stderr, "dbg2       project:              %p\n", project);
    fprintf(stderr, "dbg2       crossing_id:          %d\n", crossing_id);
    fprintf(stderr, "dbg2       offset_x:             %f\n", offset_x);
    fprintf(stderr, "dbg2       offset_y:             %f\n", offset_y);
  }

  /* get crossing */
  struct mbna_crossing *crossing =
      (struct mbna_crossing *)&project->crossings[crossing_id];

  /* get section endpoints */
  struct mbna_file *file = &project->files[crossing->file_id_1];
  struct mbna_section *section1 = &file->sections[crossing->section_1];
  file = &project->files[crossing->file_id_2];
  struct mbna_section *section2 = &file->sections[crossing->section_2];

  // find focus point - center of the line segment connecting the two closest
  // approach nav points
  int snav_1_closest = 0;
  int snav_2_closest = 0;
  double distance_closest = 999999999.999;
  for (int isnav1 = 0; isnav1 < section1->num_snav; isnav1++) {
    for (int isnav2 = 0; isnav2 < section2->num_snav; isnav2++) {
      const double dx = (section2->snav_lon[isnav2] + offset_x - section1->snav_lon[isnav1]) / project->mtodeglon;
      const double dy = (section2->snav_lat[isnav2] + offset_y - section1->snav_lat[isnav1]) / project->mtodeglat;
      const double distance = sqrt(dx * dx + dy * dy);
      if (distance < distance_closest) {
        distance_closest = distance;
        snav_1_closest = isnav1;
        snav_2_closest = isnav2;
      }
    }
  }
  *lon_focus = 0.5 * (section1->snav_lon[snav_1_closest] + section2->snav_lon[snav_2_closest]);
  *lat_focus = 0.5 * (section1->snav_lat[snav_1_closest] + section2->snav_lat[snav_2_closest]);

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       lon_focus:   %.10f\n", *lon_focus);
    fprintf(stderr, "dbg2       lat_focus:   %.10f\n", *lat_focus);
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_set_plot_functions(int verbose, struct mbna_project *project,
                             void *plot, void *newpen, void *setline,
                             void *justify_string, void *plot_string, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       plot:             %p\n", plot);
    fprintf(stderr, "dbg2       newpen:           %p\n", newpen);
    fprintf(stderr, "dbg2       setline:          %p\n", setline);
    fprintf(stderr, "dbg2       justify_string:   %p\n", justify_string);
    fprintf(stderr, "dbg2       plot_string:      %p\n", plot_string);
  }

  /* set the plotting function pointers */
  project->mbnavadjust_plot = plot;
  project->mbnavadjust_newpen = newpen;
  project->mbnavadjust_setline = setline;
  project->mbnavadjust_justify_string = justify_string;
  project->mbnavadjust_plot_string = plot_string;

  int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_read_triangles(int verbose, struct mbna_project *project,
                             int file_id, int section_id,
                             struct swath *swath, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       file_id:          %d\n", file_id);
    fprintf(stderr, "dbg2       section_id:       %d\n", section_id);
    fprintf(stderr, "dbg2       swath:            %p\n", swath);
  }

  // if all good then try to read existing triangularization
  mb_path tpath;
  sprintf(tpath, "%s/nvs_%4.4d_%4.4d.mb71.tri", project->datadir, file_id, section_id);

  int status = MB_SUCCESS;
  FILE *tfp = fopen(tpath, "r");
  if (tfp != NULL) {
    size_t read_size;

    // read the triangularization file header
    int tfile_tag;
    if ((read_size = fread(&tfile_tag, 1, sizeof(int), tfp)) != sizeof(int))
      status = MB_FAILURE;

    unsigned short tfile_version_major;
    if (status == MB_SUCCESS
        && (read_size = fread(&tfile_version_major, 1, sizeof(tfile_version_major), tfp))
        != sizeof(tfile_version_major))
      status = MB_FAILURE;

    unsigned short tfile_version_minor;
    if (status == MB_SUCCESS
        && (read_size = fread(&tfile_version_minor, 1, sizeof(tfile_version_minor), tfp))
        != sizeof(tfile_version_minor))
      status = MB_FAILURE;
    if (status == MB_SUCCESS
        && (read_size = fread(&(swath->triangle_scale), 1, sizeof(double), tfp)) != sizeof(double))
      status = MB_FAILURE;
    if (status == MB_SUCCESS
        && (read_size = fread(&(swath->npts), 1, sizeof(int), tfp)) != sizeof(int))
      status = MB_FAILURE;
    if (status == MB_SUCCESS
        && (read_size = fread(&(swath->ntri), 1, sizeof(int), tfp)) != sizeof(int))
      status = MB_FAILURE;
    if (status == MB_FAILURE) {
      *error = MB_ERROR_EOF;
    }

    // allocate memory if required
    if (status == MB_SUCCESS) {
      if (swath->npts > swath->npts_alloc) {
        swath->npts_alloc = swath->npts;
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(int), (void **)&(swath->edge), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(int), (void **)&(swath->pingid), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(int), (void **)&(swath->beamid), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(double), (void **)&(swath->x), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(double), (void **)&(swath->y), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->npts_alloc * sizeof(double), (void **)&(swath->z), error);
      }
      if (swath->ntri > swath->ntri_alloc) {
        swath->ntri_alloc = swath->ntri;
        for (int i = 0; i < 3; i++) {
          status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->iv[i]), error);
          status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->ct[i]), error);
          status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->cs[i]), error);
          status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->ed[i]), error);
          status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->flag[i]), error);
        }
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(double), (void **)&(swath->v1), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(double), (void **)&(swath->v2), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(double), (void **)&(swath->v3), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, swath->ntri * sizeof(int), (void **)&(swath->istack), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, 3 * swath->ntri * sizeof(int), (void **)&(swath->kv1), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, 3 * swath->ntri * sizeof(int), (void **)&(swath->kv2), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, (4 * swath->ntri + 1) * sizeof(double), (void **)&(swath->xsave), error);
        status &= mb_reallocd(verbose, __FILE__, __LINE__, (4 * swath->ntri + 1) * sizeof(double), (void **)&(swath->ysave), error);
      }
      if (status == MB_FAILURE) {
        *error = MB_ERROR_MEMORY_FAIL;
      }
    }

    // read the vertices and the triangles
    if (status == MB_SUCCESS) {
      for (int i=0; i<swath->npts; i++) {
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->edge[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->pingid[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->beamid[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
      }
      for (int i=0; i<swath->ntri; i++) {
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->iv[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->iv[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->iv[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ct[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ct[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ct[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->cs[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->cs[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->cs[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ed[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ed[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
        if (status == MB_SUCCESS
            && (read_size = fread(&(swath->ed[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status = MB_FAILURE;
      }
      fclose(tfp);

      if (status != MB_SUCCESS) {
        *error = MB_ERROR_EOF;
        swath->npts = 0;
        swath->ntri = 0;
      }
    }

    // get x y z values from swath->pings
    if (status == MB_SUCCESS && swath->npts > 0) {
      swath->bath_min = swath->pings[swath->pingid[0]].bath[swath->beamid[0]];;
      swath->bath_max = swath->bath_min;
      for (int ipt = 0; ipt<swath->npts; ipt++) {
        const int iping = swath->pingid[ipt];
        const int ibeam = swath->beamid[ipt];
        swath->x[ipt] = swath->pings[iping].bathlon[ibeam];
        swath->y[ipt] = swath->pings[iping].bathlat[ibeam];
        swath->z[ipt] = swath->pings[iping].bath[ibeam];
        swath->bath_min = MIN(swath->bath_min, swath->z[ipt]);
        swath->bath_max = MAX(swath->bath_max, swath->z[ipt]);
      }
    }
  }
  else {
    status = MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_write_triangles(int verbose, struct mbna_project *project,
                             int file_id, int section_id,
                             struct swath *swath, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       file_id:          %d\n", file_id);
    fprintf(stderr, "dbg2       section_id:       %d\n", section_id);
    fprintf(stderr, "dbg2       swath:            %p\n", swath);
  }

  // if all good then try to read existing triangularization
  int status = MB_SUCCESS;
  *error = MB_ERROR_NO_ERROR;
  // TODO(schwehr): Remove this redundant status chech
  if (status == MB_SUCCESS && swath->ntri > 0) {
    mb_path tpath;
    sprintf(tpath, "%s/nvs_%4.4d_%4.4d.mb71.tri", project->datadir, file_id, section_id);
    FILE *tfp = fopen(tpath, "w");
    if (tfp != NULL) {
      size_t write_size;

      // write the triangularization file header
      const int tfile_tag = 74726961;
      if ((write_size = fwrite(&tfile_tag, 1, sizeof(int), tfp)) != sizeof(int))
        status &= MB_FAILURE;
      const unsigned short tfile_version_major = 1;
      if ((write_size = fwrite(&tfile_version_major, 1, sizeof(unsigned short), tfp)) != sizeof(unsigned short))
        status &= MB_FAILURE;
      const unsigned short tfile_version_minor = 0;
      if ((write_size = fwrite(&tfile_version_minor, 1, sizeof(unsigned short), tfp)) != sizeof(unsigned short))
        status &= MB_FAILURE;
      if ((write_size = fwrite(&(swath->triangle_scale), 1, sizeof(double), tfp)) != sizeof(double))
        status &= MB_FAILURE;
      if ((write_size = fwrite(&(swath->npts), 1, sizeof(int), tfp)) != sizeof(int))
        status &= MB_FAILURE;
      if ((write_size = fwrite(&(swath->ntri), 1, sizeof(int), tfp)) != sizeof(int))
        status &= MB_FAILURE;

      // write the vertices and the triangles
      for (int i=0; i<swath->npts; i++) {
        if ((write_size = fwrite(&(swath->edge[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->pingid[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->beamid[i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
      }
      for (int i=0; i<swath->ntri; i++) {
        if ((write_size = fwrite(&(swath->iv[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->iv[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->iv[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ct[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ct[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ct[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->cs[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->cs[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->cs[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ed[0][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ed[1][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        if ((write_size = fwrite(&(swath->ed[2][i]), 1, sizeof(int), tfp)) != sizeof(int))
          status &= MB_FAILURE;
        }

      fclose (tfp);
      if (status != MB_SUCCESS)
        *error = MB_ERROR_WRITE_FAIL;
    }
  }
  else {
    status &= MB_FAILURE;
    *error = MB_ERROR_OPEN_FAIL;
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_section_load(int verbose, struct mbna_project *project,
                             int file_id, int section_id,
                             void **swathraw_ptr, void **swath_ptr, int num_pings, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       file_id:          %d\n", file_id);
    fprintf(stderr, "dbg2       section_id:       %d\n", section_id);
    fprintf(stderr, "dbg2       swathraw_ptr:     %p  %p\n", swathraw_ptr, *swathraw_ptr);
    fprintf(stderr, "dbg2       swath_ptr:        %p  %p\n", swath_ptr, *swath_ptr);
    fprintf(stderr, "dbg2       num_pings:        %d\n", num_pings);
  }

  int status = MB_SUCCESS;

  /* load specified section */
  if (project->open && project->num_crossings > 0) {
    /* set section format and path */
    char path[STRING_MAX];
    sprintf(path, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, file_id, section_id);
    char tpath[STRING_MAX];
    sprintf(tpath, "%s/nvs_%4.4d_%4.4d.mb71.tri", project->datadir, file_id, section_id);
    const int iformat = 71;
    struct mbna_file *file = &(project->files[file_id]);
    struct mbna_section *section = &(file->sections[section_id]);

    void *imbio_ptr = NULL;
    const int pings = 1;
    const int lonflip = 0;
    double bounds[4] = {-360, 360, -90, 90};
    int btime_i[7] = {1962, 2, 21, 10, 30, 0, 0};
    int etime_i[7] = {2062, 2, 21, 10, 30, 0, 0};
    double btime_d = -248016600.0;
    double etime_d = 2907743400.0;
    double speedmin = 0.0;
    double timegap = 1000000000.0;
    int beams_bath;
    int beams_amp;
    int pixels_ss;

    if ((status = mb_read_init(verbose, path, iformat, pings, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                               &imbio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) != MB_SUCCESS) {
      char *error_message;
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
      fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", path);
      exit(0);  // TODO(schwehr): Use EXIT_FAILURE
    }

    char *beamflag = NULL;
    double *bath = NULL;
    double *amp = NULL;
    double *bathacrosstrack = NULL;
    double *bathalongtrack = NULL;
    double *ss = NULL;
    double *ssacrosstrack = NULL;
    double *ssalongtrack = NULL;

    /* allocate memory for data arrays */
    if (status == MB_SUCCESS) {
      if (*error == MB_ERROR_NO_ERROR)
        status =
            mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char), (void **)&beamflag, error);
      if (*error == MB_ERROR_NO_ERROR)
        status =
            mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bath, error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp, error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                   (void **)&bathacrosstrack, error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                   (void **)&bathalongtrack, error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss, error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssacrosstrack,
                                   error);
      if (*error == MB_ERROR_NO_ERROR)
        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssalongtrack,
                                   error);

      /* if error initializing memory then don't read the file */
      if (*error != MB_ERROR_NO_ERROR) {
        char *error_message;
        mb_error(verbose, *error, &error_message);
        fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
      }
    }

    struct mbna_swathraw *swathraw;
    struct mbna_pingraw *pingraw;
    struct swath *swath;
    int contour_algorithm = MB_CONTOUR_TRIANGLES; /* not MB_CONTOUR_OLD;*/

    /* allocate memory for data arrays */
    if (status == MB_SUCCESS) {
     /* get mb_io_ptr */
      // struct mb_io_struct *imb_io_ptr = (struct mb_io_struct *)imbio_ptr;

      /* initialize data storage */
      status = mb_mallocd(verbose, __FILE__, __LINE__, sizeof(struct mbna_swathraw), (void **)swathraw_ptr, error);
      swathraw = (struct mbna_swathraw *)*swathraw_ptr;
      swathraw->beams_bath = beams_bath;
      swathraw->npings_max = num_pings;
      swathraw->npings = 0;
      status = mb_mallocd(verbose, __FILE__, __LINE__, num_pings * sizeof(struct mbna_pingraw),
                          (void **)&swathraw->pingraws, error);
      for (int i = 0; i < swathraw->npings_max; i++) {
        pingraw = &swathraw->pingraws[i];
        pingraw->beams_bath = 0;
        pingraw->beamflag = NULL;
        pingraw->bath = NULL;
        pingraw->bathacrosstrack = NULL;
        pingraw->bathalongtrack = NULL;
      }

      /* initialize contour controls */
      const double tick_len_map = MAX(section->lonmax - section->lonmin, section->latmax - section->latmin) / 500;
      const double label_hgt_map = MAX(section->lonmax - section->lonmin, section->latmax - section->latmin) / 100;
      const int contour_ncolor = 10;
      status = mb_contour_init(verbose, (struct swath **)swath_ptr, num_pings, beams_bath, contour_algorithm,
                               true, false, false, false, false, project->cont_int, project->col_int, project->tick_int,
                               project->label_int, tick_len_map, label_hgt_map, 0.0, contour_ncolor, 0, NULL, NULL, NULL, 0.0,
                               0.0, 0.0, 0.0, 0, 0, 0.0, 0.0,
                                     project->mbnavadjust_plot, project->mbnavadjust_newpen,
                                     project->mbnavadjust_setline, project->mbnavadjust_justify_string,
                                     project->mbnavadjust_plot_string,
                                     error);
      swath = (struct swath *)*swath_ptr;
      swath->beams_bath = beams_bath;
      swath->npings = 0;
      swath->triangle_scale = project->triangle_scale;

      /* if error initializing memory then quit */
      if (*error != MB_ERROR_NO_ERROR) {
        char *error_message;
        mb_error(verbose, *error, &error_message);
        fprintf(stderr, "\nMBIO Error allocating contour control structure:\n%s\n", error_message);
        fprintf(stderr, "\nProgram <%s> Terminated\n", program_name);
        exit(*error);
      }
    }

    /* now read the data */
    if (status == MB_SUCCESS) {
      struct ping *ping;

      bool done = false;
      while (!done) {
        void *istore_ptr = NULL;
        int kind;
        int time_i[7];
        double time_d;
        double navlon;
        double navlat;
        double speed;
        double heading;
        double distance;
        double altitude;
        double sonardepth;
        double roll;
        double pitch;
        double heave;
        char comment[MB_COMMENT_MAXLINE];

        /* read the next ping */
        status = mb_get_all(verbose, imbio_ptr, &istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                            &heading, &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag,
                            bath, amp, bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

        /* handle successful read */
        if (status == MB_SUCCESS && kind == MB_DATA_DATA) {
          /* allocate memory for the raw arrays */
          pingraw = &swathraw->pingraws[swathraw->npings];
          status = mb_mallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(char), (void **)&pingraw->beamflag,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double), (void **)&pingraw->bath,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double),
                              (void **)&pingraw->bathacrosstrack, error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double),
                              (void **)&pingraw->bathalongtrack, error);

          /* make sure enough memory is allocated for contouring arrays */
          ping = &swath->pings[swathraw->npings];
          if (ping->beams_bath_alloc < beams_bath) {
            status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(char),
                                 (void **)&(ping->beamflag), error);
            status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double),
                                 (void **)&(ping->bath), error);
            status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double),
                                 (void **)&(ping->bathlon), error);
            status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(double),
                                 (void **)&(ping->bathlat), error);
            if (contour_algorithm == MB_CONTOUR_OLD) {
              status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(int),
                                   (void **)&(ping->bflag[0]), error);
              status = mb_reallocd(verbose, __FILE__, __LINE__, beams_bath * sizeof(int),
                                   (void **)&(ping->bflag[1]), error);
            }
            ping->beams_bath_alloc = beams_bath;
          }

          /* copy arrays and update bookkeeping */
          if (*error == MB_ERROR_NO_ERROR) {
            swathraw->npings++;
            if (swathraw->npings >= swathraw->npings_max)
              done = true;

            for (int i = 0; i < 7; i++)
              pingraw->time_i[i] = time_i[i];
            pingraw->time_d = time_d;
            pingraw->navlon = navlon;
            pingraw->navlat = navlat;
            pingraw->heading = heading;
            pingraw->draft = sonardepth;
            pingraw->beams_bath = beams_bath;
            /* fprintf(stderr,"\nPING %d : %4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d.%6.6d\n",
            swathraw->npings,time_i[0],time_i[1],time_i[2],time_i[3],time_i[4],time_i[5],time_i[6]); */
            for (int i = 0; i < beams_bath; i++) {
              pingraw->beamflag[i] = beamflag[i];
              if (mb_beam_ok(beamflag[i])) {
                pingraw->beamflag[i] = beamflag[i];
                pingraw->bath[i] = bath[i];
                pingraw->bathacrosstrack[i] = bathacrosstrack[i];
                pingraw->bathalongtrack[i] = bathalongtrack[i];
              }
              else {
                pingraw->beamflag[i] = MB_FLAG_NULL;
                pingraw->bath[i] = 0.0;
                pingraw->bathacrosstrack[i] = 0.0;
                pingraw->bathalongtrack[i] = 0.0;
              }
              /* fprintf(stderr,"BEAM: %d:%d  Flag:%d    %f %f %f\n",
              swathraw->npings,i,pingraw->beamflag[i],pingraw->bath[i],pingraw->bathacrosstrack[i],pingraw->bathalongtrack[i]);
              */
            }
          }

          /* extract all nav values */
          status = mb_extract_nav(verbose, imbio_ptr, istore_ptr, &kind, pingraw->time_i, &pingraw->time_d,
                                  &pingraw->navlon, &pingraw->navlat, &speed, &pingraw->heading, &pingraw->draft, &roll,
                                  &pitch, &heave, error);

          /*fprintf(stderr, "%d  %4d/%2d/%2d %2d:%2d:%2d.%6.6d  %15.10f %15.10f %d:%d\n",
          status,
          ping->time_i[0],ping->time_i[1],ping->time_i[2],
          ping->time_i[3],ping->time_i[4],ping->time_i[5],ping->time_i[6],
          ping->navlon, ping->navlat, beams_bath, swath->beams_bath);*/

          if (verbose >= 2) {
            fprintf(stderr, "\ndbg2  Ping read in program <%s>\n", program_name);
            fprintf(stderr, "dbg2       kind:           %d\n", kind);
            fprintf(stderr, "dbg2       npings:         %d\n", swathraw->npings);
            fprintf(stderr, "dbg2       time:           %4d %2d %2d %2d %2d %2d %6.6d\n", pingraw->time_i[0],
                    pingraw->time_i[1], pingraw->time_i[2], pingraw->time_i[3], pingraw->time_i[4],
                    pingraw->time_i[5], pingraw->time_i[6]);
            fprintf(stderr, "dbg2       navigation:     %f  %f\n", pingraw->navlon, pingraw->navlat);
            fprintf(stderr, "dbg2       beams_bath:     %d\n", beams_bath);
            fprintf(stderr, "dbg2       beams_amp:      %d\n", beams_amp);
            fprintf(stderr, "dbg2       pixels_ss:      %d\n", pixels_ss);
            fprintf(stderr, "dbg2       done:           %d\n", done);
            fprintf(stderr, "dbg2       error:          %d\n", *error);
            fprintf(stderr, "dbg2       status:         %d\n", status);
          }
        }
        else if (*error > MB_ERROR_NO_ERROR) {
          status = MB_SUCCESS;
          *error = MB_ERROR_NO_ERROR;
          done = true;
        }
      }

      status = mb_close(verbose, &imbio_ptr, error);
    }

    // translate the raw swath data to the contouring structure
    status = mbnavadjust_section_translate(verbose, project, file_id, *swathraw_ptr, *swath_ptr, 0.0, error);

    // if all good then try to read existing triangularization
    if (status == MB_SUCCESS && swath->npings > 0) {
      status = mbnavadjust_read_triangles(verbose, project, file_id, section_id, swath, error);

      // if triangles cannot be read then make triangles and write the triangles out
      if (status == MB_FAILURE) {
        status = MB_SUCCESS;
        *error = MB_ERROR_NO_ERROR;
        fprintf(stderr, "Creating triangles for %4.4d:%2.2d\n", file_id, section_id);
        status = mb_triangulate(verbose, swath, error);
        fprintf(stderr, " - Write triangles for %4.4d:%2.2d - %d pts %d triangles\n",
                  file_id, section_id, swath->npts, swath->ntri);
        if (status == MB_SUCCESS) {
          status = mbnavadjust_write_triangles(verbose, project, file_id, section_id, swath, error);
        }
      }
      else {
        fprintf(stderr, "Read triangles for %4.4d:%2.2d - %d pts %d triangles\n",
                  file_id, section_id, swath->npts, swath->ntri);
      }
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_section_unload(int verbose, void **swathraw_ptr, void **swath_ptr, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       swathraw_ptr:     %p  %p\n", swathraw_ptr, *swathraw_ptr);
    fprintf(stderr, "dbg2       swath_ptr:        %p  %p\n", swath_ptr, *swath_ptr);
  }

  int status = MB_SUCCESS;

  /* unload specified section */
  struct mbna_swathraw *swathraw = (struct mbna_swathraw *)(*swathraw_ptr);
  struct swath *swath = (struct swath *)(*swath_ptr);

  /* free raw swath data */
  if (swathraw != NULL && swathraw->pingraws != NULL) {
    for (int i = 0; i < swathraw->npings_max; i++) {
      struct mbna_pingraw *pingraw = &swathraw->pingraws[i];
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&pingraw->beamflag, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&pingraw->bath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&pingraw->bathacrosstrack, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&pingraw->bathalongtrack, error);
    }
    status = mb_freed(verbose, __FILE__, __LINE__, (void **)&swathraw->pingraws, error);
  }
  if (swathraw != NULL)
    status = mb_freed(verbose, __FILE__, __LINE__, (void **)&swathraw, error);

  /* free contours */
  status = mb_contour_deall(verbose, swath, error);

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/

int mbnavadjust_fix_section_sensordepth(int verbose, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
  }

  int status = MB_SUCCESS;

  /* read specified section, extracting sensordepth values for the snav points */
  if (project != NULL) {
    /* mbio read and write values */
    void *imbio_ptr = NULL;
    void *istore_ptr = NULL;
    char *beamflag = NULL;
    double *bath = NULL;
    double *bathacrosstrack = NULL;
    double *bathalongtrack = NULL;
    double *amp = NULL;
    double *ss = NULL;
    double *ssacrosstrack = NULL;
    double *ssalongtrack = NULL;
    char comment[MB_COMMENT_MAXLINE];

    /* MBIO control parameters */

    int isnav;
    int num_pings;

    for (int ifile=0;ifile<project->num_files;ifile++) {
      struct mbna_file *file = &(project->files[ifile]);
      for (int isection = 0; isection<file->num_sections; isection++) {

        /* set section format and path */
        char path[STRING_MAX];
        sprintf(path, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, ifile, isection);
        const int iformat = 71;
        file = &(project->files[ifile]);
        struct mbna_section *section = &(file->sections[isection]);

        const int pings = 1;
        const int lonflip = 0;
        double bounds[4] = {-360, 360, -90, 90};
        int btime_i[7] = {1962, 2, 21, 10, 30, 0, 0};
        int etime_i[7] = {2062, 2, 21, 10, 30, 0, 0};
        double btime_d = -248016600.0;
        double etime_d = 2907743400.0;
        const double speedmin = 0.0;
        const double timegap = 1000000000.0;
        int beams_bath;
        int beams_amp;
        int pixels_ss;

                /* initialize section for reading */
                if ((status = mb_read_init(verbose, path, iformat, pings, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                                           &imbio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) != MB_SUCCESS) {
                  char *error_message;
                    mb_error(verbose, *error, &error_message);
                    fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
                    fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", path);
                    exit(0);
                }

                /* allocate memory for data arrays */
                if (status == MB_SUCCESS) {
                    if (*error == MB_ERROR_NO_ERROR)
                        status =
                            mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char), (void **)&beamflag, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status =
                            mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bath, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                                   (void **)&bathacrosstrack, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                                   (void **)&bathalongtrack, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss, error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssacrosstrack,
                                                   error);
                    if (*error == MB_ERROR_NO_ERROR)
                        status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssalongtrack,
                                                   error);

                    /* if error initializing memory then don't read the file */
                    if (*error != MB_ERROR_NO_ERROR) {
                        char *error_message;
                        mb_error(verbose, *error, &error_message);
                        fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
                    }
                }

                /* now read the data */
                if (status == MB_SUCCESS) {
		    // struct mb_io_struct *imb_io_ptr = (struct mb_io_struct *)imbio_ptr;
                    bool done = false;
                    isnav = 0;
                    num_pings = 0;
                    while (!done && isnav < section->num_snav) {
                      int kind;
                      int time_i[7];
                      double time_d;
                      double navlon;
                      double navlat;
                      double speed;
                      double heading;
                      double distance;
                      double altitude;
                      double sonardepth;
                        /* read the next ping */
                        status = mb_get_all(verbose, imbio_ptr, &istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                                            &heading, &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag,
                                            bath, amp, bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

                        /* handle successful read */
                        if (status == MB_SUCCESS && kind == MB_DATA_DATA) {
                            if (num_pings == section->snav_id[isnav]) {
                                section->snav_sensordepth[isnav] = sonardepth;
fprintf(stderr, "Update sensordepth section %4.4d:%4.4d:%2.2d  %4d/%2d/%2d %2d:%2d:%2d.%6.6d  %.6f %.6f %.6f\n",
ifile, isection, isnav, time_i[0],time_i[1],time_i[2],time_i[3],time_i[4],time_i[5],time_i[6],
time_d, section->snav_time_d[isnav], (section->snav_time_d[isnav]-time_d));
                                isnav++;
                            }
                            num_pings++;
                        }
                        else if (*error > MB_ERROR_NO_ERROR) {
                            status = MB_SUCCESS;
                            *error = MB_ERROR_NO_ERROR;
                            done = true;
                        }
                    }

                    /* close the input data file */
                    status = mb_close(verbose, &imbio_ptr, error);
                }
            }
        }
    }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_section_translate(int verbose, struct mbna_project *project,
                                  int file_id, void *swathraw_ptr, void *swath_ptr,
                                  double zoffset, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       file_id:          %d\n", file_id);
    fprintf(stderr, "dbg2       swathraw_ptr:     %p\n", swathraw_ptr);
    fprintf(stderr, "dbg2       swath_ptr:        %p\n", swath_ptr);
    fprintf(stderr, "dbg2       zoffset:          %f\n", zoffset);
  }

  // translate soundings from the raw swath form to the structure used for
  // contouring, including applying a depth offset and any heading or roll bias
  // applied to the source data on a file by file basis
  if (project != NULL && swathraw_ptr != NULL && swath_ptr != NULL && project->open) {
    struct mbna_swathraw *swathraw = (struct mbna_swathraw *)swathraw_ptr;
    struct swath *swath = (struct swath *)swath_ptr;
    swath->npings = 0;
    bool first = true;
    swath->bath_min = 0.0;
    swath->bath_max = 0.0;

    for (int iping = 0; iping < swathraw->npings; iping++) {
      swath->npings++;
      struct mbna_pingraw *pingraw = &swathraw->pingraws[iping];
      struct ping *ping = &swath->pings[swath->npings - 1];
      for (int i = 0; i < 7; i++)
        ping->time_i[i] = pingraw->time_i[i];
      ping->time_d = pingraw->time_d;
      ping->navlon = pingraw->navlon;
      ping->navlat = pingraw->navlat;
      ping->heading = pingraw->heading + project->files[file_id].heading_bias;
      double mtodeglon;
      double mtodeglat;
      mb_coor_scale(verbose, pingraw->navlat, &mtodeglon, &mtodeglat);
      const double headingx = sin(ping->heading * DTR);
      const double headingy = cos(ping->heading * DTR);
      ping->beams_bath = pingraw->beams_bath;
      for (int i = 0; i < ping->beams_bath; i++) {
        if (mb_beam_ok(pingraw->beamflag[i])) {
          /* strip off transducer depth */
          double depth = pingraw->bath[i] - pingraw->draft;

          // get range and angles in roll-pitch frame
          const double range = sqrt(depth * depth + pingraw->bathacrosstrack[i] * pingraw->bathacrosstrack[i] +
                       pingraw->bathalongtrack[i] * pingraw->bathalongtrack[i]);
          const double alpha = asin(pingraw->bathalongtrack[i] / range);
          double beta = acos(pingraw->bathacrosstrack[i] / range / cos(alpha));

          /* apply roll correction */
          beta += DTR * project->files[file_id].roll_bias;

          /* recalculate bathymetry */
          depth = range * cos(alpha) * sin(beta);
          const double depthalongtrack = range * sin(alpha);
          const double depthacrosstrack = range * cos(alpha) * cos(beta);

          /* add heave and draft back in */
          depth += pingraw->draft;

          /* add zoffset */
          depth += zoffset;

          /* get bathymetry in lon lat */
          ping->beamflag[i] = pingraw->beamflag[i];
          ping->bath[i] = depth;
          ping->bathlon[i] = pingraw->navlon
                              + headingy * mtodeglon * depthacrosstrack
                              + headingx * mtodeglon * depthalongtrack;
          ping->bathlat[i] = pingraw->navlat
                              - headingx * mtodeglat * depthacrosstrack
                              + headingy * mtodeglat * depthalongtrack;

          if (first) {
            swath->bath_min = depth;
            swath->bath_max = depth;
            first = false;
          } else {
            swath->bath_min = MIN(depth, swath->bath_min);
            swath->bath_max = MAX(depth, swath->bath_max);
          }
        }
        else {
          ping->beamflag[i] = MB_FLAG_NULL;
          ping->bath[i] = 0.0;
          ping->bathlon[i] = pingraw->navlon;
          ping->bathlat[i] = pingraw->navlat;
        }
      }
    }

    // if soundings have been triangulated then reset the x y z values for the vertices
    if (swath->npts > 0) {
      for (int ipt=0; ipt<swath->npts; ipt++) {
        struct ping *ping = &swath->pings[swath->pingid[ipt]];
        swath->x[ipt] = ping->bathlon[swath->beamid[ipt]];
        swath->y[ipt] = ping->bathlat[swath->beamid[ipt]];
        swath->z[ipt] = ping->bath[swath->beamid[ipt]];
      }
    }

  }

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_section_contour(int verbose, struct mbna_project *project,
                                int fileid, int sectionid, struct swath *swath,
                                struct mbna_contour_vector *contour, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       fileid:           %d\n", fileid);
    fprintf(stderr, "dbg2       sectionid:        %d\n", sectionid);
    fprintf(stderr, "dbg2       swath:            %p\n", swath);
    fprintf(stderr, "dbg2       contour:          %p\n", contour);
    fprintf(stderr, "dbg2       nvector:          %d\n", contour->nvector);
    fprintf(stderr, "dbg2       nvector_alloc:    %d\n", contour->nvector_alloc);
  }

  int status = MB_SUCCESS;

  if (swath != NULL) {
    /* set vectors */
    contour->nvector = 0;

    /* reset contouring parameters */
    swath->contour_int = project->cont_int;
    swath->color_int = project->col_int;
    swath->tick_int = project->tick_int;

    /* generate contours */
    status = mb_contour(verbose, swath, error);

    /* set contours up to date flag */
    project->files[fileid].sections[sectionid].contoursuptodate = true;
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_import_data(int verbose, struct mbna_project *project,
                                char *path, int iformat, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       path:             %s\n", path);
    fprintf(stderr, "dbg2       format:           %d\n", iformat);
  }

  int status = MB_SUCCESS;
  struct mbna_file *file;
  struct mbna_section *section;
  char filename[STRING_MAX];
  char dfile[STRING_MAX];
  double weight;
  int form;
  void *datalist;

  /* loop until all files read */
  bool done = false;
  bool firstfile = true;
  while (!done) {
    if (iformat > 0) {
      status = mbnavadjust_import_file(verbose, project, path, iformat, firstfile, error);
      done = true;
      firstfile = false;
    }
    else if (iformat == -1) {
      if ((status = mb_datalist_open(verbose, &datalist, path, MB_DATALIST_LOOK_NO, error)) == MB_SUCCESS) {
        while (!done) {
          if ((status = mb_datalist_read(verbose, datalist, filename, dfile, &form, &weight, error)) ==
              MB_SUCCESS) {
            status = mbnavadjust_import_file(verbose, project, filename, form, firstfile, error);
            firstfile = false;
          }
          else {
            mb_datalist_close(verbose, &datalist, error);
            done = true;
          }
        }
      }
    }
  }

  /* look for new crossings */
  status = mbnavadjust_findcrossings(verbose, project, error);

  /* count the number of blocks */
  project->num_surveys = 0;
  for (int i = 0; i < project->num_files; i++) {
    file = &project->files[i];
    if (i == 0 || !file->sections[0].continuity) {
      project->num_surveys++;
    }
    file->block = project->num_surveys - 1;
    file->block_offset_x = 0.0;
    file->block_offset_y = 0.0;
    file->block_offset_z = 0.0;
  }

  /* set project bounds and scaling */
  for (int i = 0; i < project->num_files; i++) {
    file = &project->files[i];
    for (int j = 0; j < file->num_sections; j++) {
      section = &file->sections[j];
      if (i == 0 && j == 0) {
        project->lon_min = section->lonmin;
        project->lon_max = section->lonmax;
        project->lat_min = section->latmin;
        project->lat_max = section->latmax;
      }
      else {
        project->lon_min = MIN(project->lon_min, section->lonmin);
        project->lon_max = MAX(project->lon_max, section->lonmax);
        project->lat_min = MIN(project->lat_min, section->latmin);
        project->lat_max = MAX(project->lat_max, section->latmax);
      }
    }
  }
  mb_coor_scale(verbose, 0.5 * (project->lat_min + project->lat_max), &project->mtodeglon, &project->mtodeglat);

  /* write updated project */
  mbnavadjust_write_project(verbose, project, error);
  project->save_count = 0;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_import_file(int verbose, struct mbna_project *project,
                                char *path, int iformat, bool firstfile, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       path:             %s\n", path);
    fprintf(stderr, "dbg2       format:           %d\n", iformat);
    fprintf(stderr, "dbg2       firstfile:        %d\n", firstfile);
  }

  int status = MB_SUCCESS;
  char *error_message;
  char ipath[STRING_MAX];
  char mb_suffix[STRING_MAX];
  int iform;
  int format_error = MB_ERROR_NO_ERROR;

  /* get potential processed file name - if success ipath is fileroot string */
  if (mb_get_format(verbose, path, ipath, &iform, &format_error) != MB_SUCCESS) {
    strcpy(ipath, path);
  }
  sprintf(mb_suffix, "p.mb%d", iformat);
  strcat(ipath, mb_suffix);

  struct stat file_status;
  char npath[STRING_MAX];
  char opath[STRING_MAX];

  /* MBIO control parameters */
  const int pings = 1;
  const int lonflip = 0;
  double bounds[4] = {-360, 360, -90, 90};
  int btime_i[7] = {1962, 2, 21, 10, 30, 0, 0};
  int etime_i[7] = {2062, 2, 21, 10, 30, 0, 0};
  double btime_d = -248016600.0;
  double etime_d = 2907743400.0;
  double speedmin = 0.0;
  double timegap = 1000000000.0;

  /* mbio read and write values */
  void *imbio_ptr = NULL;
  void *ombio_ptr = NULL;
  void *istore_ptr = NULL;
  void *ostore_ptr = NULL;
  int kind = MB_DATA_NONE;
  int time_i[7];
  double time_d;
  double navlon;
  double navlat;
  double speed;
  double heading;
  double distance;
  double altitude;
  double sonardepth;
  double draft;
  double roll;
  double pitch;
  double heave;
  int beams_bath = 0;
  int beams_amp = 0;
  int pixels_ss = 0;
  char comment[MB_COMMENT_MAXLINE];

  int sonartype = MB_TOPOGRAPHY_TYPE_UNKNOWN;
    int sensorhead = 0;
  int *bin_nbath = NULL;
  double *bin_bath = NULL;
  double *bin_bathacrosstrack = NULL;
  double *bin_bathalongtrack = NULL;
  int side;
  double port_time_d, stbd_time_d;
  double angle, dt, alongtrackdistance, xtrackavg, xtrackmax;
  int nxtrack;

  int obeams_bath, obeams_amp, opixels_ss;
  int nread;
  bool first;
  double headingx, headingy, mtodeglon, mtodeglat;
  double lon, lat;
  double navlon_old, navlat_old, sensordepth_old;
  FILE *nfp;
  struct mbna_file *file = NULL;
  struct mbna_section *section = NULL;
  struct mbna_file *cfile = NULL;
  struct mbna_section *csection = NULL;
  struct mbsys_ldeoih_struct *ostore = NULL;
  struct mb_io_struct *omb_io_ptr = NULL;
  double dx1, dy1;
  int mbp_heading_mode;
  double mbp_headingbias;
  int mbp_rollbias_mode;
  double mbp_rollbias;
  double mbp_rollbias_port;
  double mbp_rollbias_stbd;
  double depthmax, distmax, depthscale, distscale;
  int error_sensorhead = MB_ERROR_NO_ERROR;
  void *tptr;
  int ii1, jj1;

  /* look for processed file and use if available */
  const int fstat = stat(ipath, &file_status);
  if (fstat != 0 || (file_status.st_mode & S_IFMT) == S_IFDIR) {
    strcpy(ipath, path);
  }

  /* turn on message */
  char *root = (char *)strrchr(ipath, '/');
  if (root == NULL)
    root = ipath;
  char message[STRING_MAX];
  sprintf(message, "Importing format %d data from %s", iformat, root);
  fprintf(stderr, "%s\n", message);
  bool output_open = false;
  project->inversion_status = MBNA_INVERSION_NONE;
  project->grid_status = MBNA_GRID_OLD;
  int new_sections = 0;
  int new_pings = 0;
  int new_crossings = 0;
  int good_beams = 0;

  /* allocate mbna_file array if needed */
  if (project->num_files_alloc <= project->num_files) {
    tptr = realloc(project->files, sizeof(struct mbna_file) * (project->num_files_alloc + ALLOC_NUM));
    if (tptr != NULL) {
      project->files = (struct mbna_file *)tptr;
      project->num_files_alloc += ALLOC_NUM;
    }
    else {
      if (project->files != NULL)
        free(project->files);
      status = MB_FAILURE;
      *error = MB_ERROR_MEMORY_FAIL;
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nError in function <%s>:\n%s\n", __func__, error_message);
      fprintf(stderr, "\nImportation of <%s> not attempted\n", ipath);
    }
  }

  if (status == MB_SUCCESS) {
    /* initialize reading the swath file */
    if ((status = mb_read_init(verbose, ipath, iformat, pings, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                               &imbio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) != MB_SUCCESS) {
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
      fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", path);
    }
  }

  char *beamflag = NULL;
  double *bath = NULL;
  double *bathacrosstrack = NULL;
  double *bathalongtrack = NULL;
  double *amp = NULL;
  double *ss = NULL;
  double *ssacrosstrack = NULL;
  double *ssalongtrack = NULL;

  /* allocate memory for data arrays */
  if (status == MB_SUCCESS) {
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char), (void **)&beamflag, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bath, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bathacrosstrack,
                                 error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bathalongtrack,
                                 error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss, error);
    if (*error == MB_ERROR_NO_ERROR)
      status =
          mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssacrosstrack, error);
    if (*error == MB_ERROR_NO_ERROR)
      status =
          mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssalongtrack, error);
    if (*error == MB_ERROR_NO_ERROR)

    /* if error initializing memory then don't read the file */
    if (*error != MB_ERROR_NO_ERROR) {
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
    }
  }

  /* open nav file */
  if (status == MB_SUCCESS) {
    sprintf(npath, "%s/nvs_%4.4d.mb166", project->datadir, project->num_files);
    if ((nfp = fopen(npath, "w")) == NULL) {
      status = MB_FAILURE;
      *error = MB_ERROR_OPEN_FAIL;
    }
  }

  /* read data */
  if (status == MB_SUCCESS) {
    nread = 0;
    bool new_section = false;
    first = true;
    while (*error <= MB_ERROR_NO_ERROR) {
      /* read a ping of data */
      status = mb_get_all(verbose, imbio_ptr, &istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed, &heading,
                          &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag, bath, amp,
                          bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

      /* extract all nav values */
      if (*error == MB_ERROR_NO_ERROR && (kind == MB_DATA_NAV || kind == MB_DATA_DATA)) {
        status = mb_extract_nav(verbose, imbio_ptr, istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                                &heading, &draft, &roll, &pitch, &heave, error);
      }

      /* ignore minor errors - use these data */
      if (kind == MB_DATA_DATA && (*error == MB_ERROR_TIME_GAP || *error == MB_ERROR_OUT_BOUNDS ||
                                   *error == MB_ERROR_OUT_TIME || *error == MB_ERROR_SPEED_TOO_SMALL)) {
        status = MB_SUCCESS;
        *error = MB_ERROR_NO_ERROR;
      }

      /* do not use data with null navigation or ludricous timestamp */
      if (kind == MB_DATA_DATA) {
        if (navlon == 0.0 && navlat == 0.0)
          *error = MB_ERROR_IGNORE;
        if (time_d <= 0.0 || time_i[0] < 1962 || time_i[0] > 3000)
          *error = MB_ERROR_IGNORE;
      }

            /* deal with survey data */
      if (kind == MB_DATA_DATA) {
        /* int status_sensorhead = */
        mb_sensorhead(verbose, imbio_ptr, istore_ptr, &sensorhead, &error_sensorhead);
        if (sonartype == MB_TOPOGRAPHY_TYPE_UNKNOWN) {
          status = mb_sonartype(verbose, imbio_ptr, istore_ptr, &sonartype, error);
        }

                /* if sonar is interferometric, bin the bathymetry */
        if (sonartype == MB_TOPOGRAPHY_TYPE_INTERFEROMETRIC) {
          /* allocate bin arrays if needed */
          if (bin_nbath == NULL) {
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(int),
                                (void **)&bin_nbath, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bath, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bathacrosstrack, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bathalongtrack, error);
            for (int i = 0; i < project->bin_beams_bath; i++) {
              bin_nbath[i] = 0;
              bin_bath[i] = 0.0;
              bin_bathacrosstrack[i] = 0.0;
              bin_bathalongtrack[i] = 0.0;
            }
          }

          /* figure out if this is a ping to one side or across the whole swath */
          xtrackavg = 0.0;
          xtrackmax = 0.0;
          nxtrack = 0;
          for (int i = 0; i < beams_bath; i++) {
            if (mb_beam_ok(beamflag[i])) {
              xtrackavg += bathacrosstrack[i];
              xtrackmax = MAX(xtrackmax, fabs(bathacrosstrack[i]));
              nxtrack++;
            }
          }
          if (nxtrack > 0) {
            xtrackavg /= nxtrack;
          }
          if (xtrackavg > 0.25 * xtrackmax) {
            side = SIDE_STBD;
            port_time_d = time_d;
          }
          else if (xtrackavg < -0.25 * xtrackmax) {
            side = SIDE_PORT;
            stbd_time_d = time_d;
          }
          else {
            side = SIDE_FULLSWATH;
            stbd_time_d = time_d;
          }

          /* if side = PORT or FULLSWATH then initialize bin arrays */
          if (side == SIDE_PORT || side == SIDE_FULLSWATH) {
            for (int i = 0; i < project->bin_beams_bath; i++) {
              bin_nbath[i] = 0;
              bin_bath[i] = 0.0;
              bin_bathacrosstrack[i] = 0.0;
              bin_bathalongtrack[i] = 0.0;
            }
          }

          /* bin the bathymetry */
          for (int i = 0; i < beams_bath; i++) {
            if (mb_beam_ok(beamflag[i])) {
              /* get apparent acrosstrack beam angle and bin accordingly */
              angle = RTD * atan(bathacrosstrack[i] / (bath[i] - sonardepth));
              const int j = (int)floor((angle + 0.5 * project->bin_swathwidth + 0.5 * project->bin_pseudobeamwidth) /
                             project->bin_pseudobeamwidth);
              /* fprintf(stderr,"i:%d bath:%f %f %f sonardepth:%f angle:%f j:%d\n",
              i,bath[i],bathacrosstrack[i],bathalongtrack[i],sonardepth,angle,j); */
              if (j >= 0 && j < project->bin_beams_bath) {
                bin_bath[j] += bath[i];
                bin_bathacrosstrack[j] += bathacrosstrack[i];
                bin_bathalongtrack[j] += bathalongtrack[i];
                bin_nbath[j]++;
              }
            }
          }

          /* if side = STBD or FULLSWATH calculate output bathymetry
              - add alongtrack offset to port data from previous ping */
          if (side == SIDE_STBD || side == SIDE_FULLSWATH) {
            dt = port_time_d - stbd_time_d;
            if (dt > 0.0 && dt < 0.5)
              alongtrackdistance = -(port_time_d - stbd_time_d) * speed / 3.6;
            else
              alongtrackdistance = 0.0;
            beams_bath = project->bin_beams_bath;
            for (int j = 0; j < project->bin_beams_bath; j++) {
              /* fprintf(stderr,"j:%d angle:%f n:%d bath:%f %f %f\n",
              j,j*project->bin_pseudobeamwidth - 0.5 *
              project->bin_swathwidth,bin_nbath[j],bin_bath[j],bin_bathacrosstrack[j],bin_bathalongtrack[j]); */
              if (bin_nbath[j] > 0) {
                bath[j] = bin_bath[j] / bin_nbath[j];
                bathacrosstrack[j] = bin_bathacrosstrack[j] / bin_nbath[j];
                bathalongtrack[j] = bin_bathalongtrack[j] / bin_nbath[j];
                beamflag[j] = MB_FLAG_NONE;
                if (bin_bathacrosstrack[j] < 0.0)
                  bathalongtrack[j] += alongtrackdistance;
              }
              else {
                beamflag[j] = MB_FLAG_NULL;
                bath[j] = 0.0;
                bathacrosstrack[j] = 0.0;
                bathalongtrack[j] = 0.0;
              }
            }
          }

          /* if side = PORT set nonfatal error so that bathymetry isn't output until
              the STBD data are read too */
          else if (side == SIDE_PORT) {
            *error = MB_ERROR_IGNORE;
          }
        }
      }

      /* deal with new file */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && first) {
        file = &project->files[project->num_files];
        file->status = MBNA_FILE_GOODNAV;
        file->id = project->num_files;
        file->output_id = 0;
        strcpy(file->path, path);
        strcpy(file->file, path);
        mb_get_relative_path(verbose, file->file, project->path, error);
        file->format = iformat;
        file->heading_bias = 0.0;
        file->roll_bias = 0.0;
        file->num_snavs = 0;
        file->num_pings = 0;
        file->num_beams = 0;
        file->num_sections = 0;
        file->num_sections_alloc = 0;
        file->sections = NULL;
        project->num_files++;
        new_section = true;
        first = false;

        /* get bias values */
        mb_pr_get_heading(verbose, file->path, &mbp_heading_mode, &mbp_headingbias, error);
        mb_pr_get_rollbias(verbose, file->path, &mbp_rollbias_mode, &mbp_rollbias, &mbp_rollbias_port,
                           &mbp_rollbias_stbd, error);
        if (mbp_heading_mode == MBP_HEADING_OFFSET || mbp_heading_mode == MBP_HEADING_CALCOFFSET) {
          file->heading_bias_import = mbp_headingbias;
        }
        else {
          file->heading_bias_import = 0.0;
        }
        if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE) {
          file->roll_bias_import = mbp_rollbias;
        }
        else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE) {
          file->roll_bias_import = 0.5 * (mbp_rollbias_port + mbp_rollbias_stbd);
        }
        else {
          file->roll_bias_import = 0.0;
        }
      }

      /* check if new section needed */
      else if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && file != NULL
               && (section->distance + distance >= project->section_length ||
                section->num_beams >= project->section_soundings)) {
        new_section = true;
        /*fprintf(stderr, "NEW SECTION: section->distance:%f distance:%f project->section_length:%f\n",
        section->distance, distance, project->section_length);*/
      }

      /* if end of section or end of file resolve position
          of last snav point in last section */
      if ((error > MB_ERROR_NO_ERROR || new_section) && project->num_files > 0
          && file != NULL && (file->num_sections > 0 && section->num_pings > 0)) {
        /* resolve position of last snav point in last section */
        if (section->num_snav == 1 ||
            (section->distance >= (section->num_snav - 0.5) * project->section_length / (MBNA_SNAV_NUM - 1))) {
          section->snav_id[section->num_snav] = section->num_pings - 1;
          section->snav_num_ties[section->num_snav] = 0;
          section->snav_distance[section->num_snav] = section->distance;
          section->snav_time_d[section->num_snav] = section->etime_d;
          section->snav_lon[section->num_snav] = navlon_old;
          section->snav_lat[section->num_snav] = navlat_old;
          section->snav_sensordepth[section->num_snav] = sensordepth_old;
          section->snav_lon_offset[section->num_snav] = 0.0;
          section->snav_lat_offset[section->num_snav] = 0.0;
          section->snav_z_offset[section->num_snav] = 0.0;
          section->num_snav++;
          file->num_snavs++;
          project->num_snavs++;
        }
        else if (section->num_snav > 1) {
          section->snav_id[section->num_snav - 1] = section->num_pings - 1;
          section->snav_num_ties[section->num_snav] = 0;
          section->snav_distance[section->num_snav - 1] = section->distance;
          section->snav_time_d[section->num_snav - 1] = section->etime_d;
          section->snav_lon[section->num_snav - 1] = navlon_old;
          section->snav_lat[section->num_snav - 1] = navlat_old;
          section->snav_sensordepth[section->num_snav - 1] = sensordepth_old;
          section->snav_lon_offset[section->num_snav - 1] = 0.0;
          section->snav_lat_offset[section->num_snav - 1] = 0.0;
          section->snav_z_offset[section->num_snav - 1] = 0.0;
        }
      }

      /* deal with new section */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && new_section) {
        /* end old section */
        if (output_open) {
          /* close the swath file */
          status = mb_close(verbose, &ombio_ptr, error);
          output_open = false;
        }

        /* allocate mbna_section array if needed */
        if (file->num_sections_alloc <= file->num_sections) {
          file->sections = (struct mbna_section *)realloc(file->sections, sizeof(struct mbna_section) *
                                                                              (file->num_sections_alloc + ALLOC_NUM));
          if (file->sections != NULL)
            file->num_sections_alloc += ALLOC_NUM;
          else {
            status = MB_FAILURE;
            *error = MB_ERROR_MEMORY_FAIL;
          }
        }

        /* initialize new section */
        file->num_sections++;
        new_sections++;
        section = &file->sections[file->num_sections - 1];
        section->num_pings = 0;
        section->num_beams = 0;
        section->continuity = false;
        section->global_start_ping = project->num_pings;
        section->global_start_snav = project->num_snavs;
        for (int i = 0; i < MBNA_MASK_DIM * MBNA_MASK_DIM; i++)
          section->coverage[i] = 0;
        section->num_snav = 0;
        if (file->num_sections > 1) {
          csection = &file->sections[file->num_sections - 2];
          if (fabs(time_d - csection->etime_d) < MBNA_TIME_GAP_MAX) {
            section->continuity = true;
            section->global_start_snav--;
            file->num_snavs--;
            project->num_snavs--;
          }
        }
        else if (project->num_files > 1 && !firstfile) {
          cfile = &project->files[project->num_files - 2];
          csection = &cfile->sections[cfile->num_sections - 1];
          if (fabs(time_d - csection->etime_d) < MBNA_TIME_GAP_MAX) {
            section->continuity = true;
            section->global_start_snav--;
            file->num_snavs--;
            project->num_snavs--;
          }
        }
        section->distance = 0.0;
        section->btime_d = time_d;
        section->etime_d = time_d;
        section->lonmin = navlon;
        section->lonmax = navlon;
        section->latmin = navlat;
        section->latmax = navlat;
        section->depthmin = 0.0;
        section->depthmax = 0.0;
        section->contoursuptodate = false;
        section->global_tie_status = MBNA_TIE_NONE;
        section->global_tie_snav = MBNA_SELECT_NONE;
        section->offset_x = 0.0;
        section->offset_y = 0.0;
        section->offset_x_m = 0.0;
        section->offset_y_m = 0.0;
        section->offset_z_m = 0.0;
        section->xsigma = 0.0;
        section->ysigma = 0.0;
        section->zsigma = 0.0;
        section->inversion_offset_x = 0.0;
        section->inversion_offset_y = 0.0;
        section->inversion_offset_x_m = 0.0;
        section->inversion_offset_y_m = 0.0;
        section->inversion_offset_z_m = 0.0;
        section->dx_m = 0.0;
        section->dy_m = 0.0;
        section->dz_m = 0.0;
        section->sigma_m = 0.0;
        section->dr1_m = 0.0;
        section->dr2_m = 0.0;
        section->dr3_m = 0.0;
        section->rsigma_m = 0.0;
        new_section = false;

        /* open output section file */
        sprintf(opath, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, file->id, file->num_sections - 1);
        if ((status = mb_write_init(verbose, opath, 71, &ombio_ptr, &obeams_bath, &obeams_amp, &opixels_ss,
                                    error)) != MB_SUCCESS) {
          mb_error(verbose, *error, &error_message);
          fprintf(stderr, "\nMBIO Error returned from function <mb_write_init>:\n%s\n", error_message);
          fprintf(stderr, "\nSwath sonar File <%s> not initialized for writing\n", path);
        }
        else {
          omb_io_ptr = (struct mb_io_struct *)ombio_ptr;
          ostore_ptr = omb_io_ptr->store_data;
          ostore = (struct mbsys_ldeoih_struct *)ostore_ptr;
          ostore->kind = MB_DATA_DATA;
          ostore->beams_bath = obeams_bath;
          ostore->beams_amp = 0;
          ostore->pixels_ss = 0;
                    ostore->sensorhead = sensorhead;
                    ostore->topo_type = sonartype;
          output_open = true;
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(char), (void **)&ostore->beamflag,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double), (void **)&ostore->bath,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double),
                              (void **)&ostore->bath_acrosstrack, error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double),
                              (void **)&ostore->bath_alongtrack, error);

          /* if error initializing memory then don't write the file */
          if (*error != MB_ERROR_NO_ERROR) {
            mb_error(verbose, *error, &error_message);
            fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->beamflag, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_acrosstrack, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_alongtrack, error);
            status = mb_close(verbose, &ombio_ptr, error);
            output_open = false;
          }
        }
      }

      /* update section distance for each data ping */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && section->num_pings > 1)
        section->distance += distance;

      /* handle good bathymetry */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR) {
        /* get statistics */
        mb_coor_scale(verbose, navlat, &mtodeglon, &mtodeglat);
        headingx = sin(DTR * heading);
        headingy = cos(DTR * heading);
        navlon_old = navlon;
        navlat_old = navlat;
        sensordepth_old = draft - heave;
        section->etime_d = time_d;
        section->num_pings++;
        file->num_pings++;
        project->num_pings++;
        new_pings++;
        if (section->distance >= section->num_snav * project->section_length / (MBNA_SNAV_NUM - 1)) {
          section->snav_id[section->num_snav] = section->num_pings - 1;
          section->snav_num_ties[section->num_snav] = 0;
          section->snav_distance[section->num_snav] = section->distance;
          section->snav_time_d[section->num_snav] = time_d;
          section->snav_lon[section->num_snav] = navlon;
          section->snav_lat[section->num_snav] = navlat;
          section->snav_sensordepth[section->num_snav] = draft - heave;;
          section->snav_lon_offset[section->num_snav] = 0.0;
          section->snav_lat_offset[section->num_snav] = 0.0;
          section->snav_z_offset[section->num_snav] = 0.0;
          section->num_snav++;
          file->num_snavs++;
          project->num_snavs++;
        }
        for (int i = 0; i < beams_bath; i++) {
          if (mb_beam_ok(beamflag[i]) && bath[i] != 0.0) {
            good_beams++;
            project->num_beams++;
            file->num_beams++;
            section->num_beams++;
            lon = navlon + headingy * mtodeglon * bathacrosstrack[i] + headingx * mtodeglon * bathalongtrack[i];
            lat = navlat - headingx * mtodeglat * bathacrosstrack[i] + headingy * mtodeglat * bathalongtrack[i];
            if (lon != 0.0)
              section->lonmin = MIN(section->lonmin, lon);
            if (lon != 0.0)
              section->lonmax = MAX(section->lonmax, lon);
            if (lat != 0.0)
              section->latmin = MIN(section->latmin, lat);
            if (lat != 0.0)
              section->latmax = MAX(section->latmax, lat);
            if (section->depthmin == 0.0)
              section->depthmin = bath[i];
            else
              section->depthmin = MIN(section->depthmin, bath[i]);
            if (section->depthmin == 0.0)
              section->depthmax = bath[i];
            else
              section->depthmax = MAX(section->depthmax, bath[i]);
          }
          else {
            beamflag[i] = MB_FLAG_NULL;
            bath[i] = 0.0;
            bathacrosstrack[i] = 0.0;
            bathalongtrack[i] = 0.0;
          }
        }

        /* write out bath data only to format 71 file */
        if (output_open) {

          /* get depth and distance scaling */
          depthmax = 0.0;
          distmax = 0.0;
          for (int i = 0; i < beams_bath; i++) {
            depthmax = MAX(depthmax, fabs(bath[i]));
            distmax = MAX(distmax, fabs(bathacrosstrack[i]));
            distmax = MAX(distmax, fabs(bathalongtrack[i]));
          }
          depthscale = MAX(0.001, depthmax / 32000);
          distscale = MAX(0.001, distmax / 32000);
          ostore->depth_scale = depthscale;
          ostore->distance_scale = distscale;
          ostore->sonardepth = draft - heave;
          ostore->roll = roll;
          ostore->pitch = pitch;
          ostore->heave = heave;

          /* write out data */
          status = mb_put_all(verbose, ombio_ptr, ostore_ptr,
                                        true, MB_DATA_DATA, time_i, time_d, navlon, navlat,
                              speed, heading, beams_bath, 0, 0,
                                        beamflag, bath, amp,
                                        bathacrosstrack, bathalongtrack,
                              ss, ssacrosstrack, ssalongtrack, comment, error);
        }

        /* write out nav data associated with bath to format 166 file */
        if (nfp != NULL) {
          fprintf(nfp, "%4.4d %2.2d %2.2d %2.2d %2.2d %2.2d.%6.6d %16.6f %.10f %.10f %.2f %.2f %.2f %.2f %.2f %.2f\r\n",
                  time_i[0], time_i[1], time_i[2], time_i[3], time_i[4], time_i[5], time_i[6], time_d, navlon, navlat,
                  heading, speed, draft, roll, pitch, heave);
        }
      }

      /* increment counters */
      if (*error == MB_ERROR_NO_ERROR)
        nread++;

      /* print debug statements */
      if (verbose >= 2) {
        fprintf(stderr, "\ndbg2  Ping read in program <%s>\n", program_name);
        fprintf(stderr, "dbg2       kind:           %d\n", kind);
        fprintf(stderr, "dbg2       error:          %d\n", *error);
        fprintf(stderr, "dbg2       status:         %d\n", status);
      }
      if (verbose >= 2 && kind == MB_DATA_COMMENT) {
        fprintf(stderr, "dbg2       comment:        %s\n", comment);
      }
      if (verbose >= 2 && error <= 0 && kind == MB_DATA_DATA) {
        fprintf(stderr, "dbg2       time_i:         %4d/%2d/%2d %2.2d:%2.2d:%2.2d.%6.6d\n", time_i[0], time_i[1],
                time_i[2], time_i[3], time_i[4], time_i[5], time_i[6]);
        fprintf(stderr, "dbg2       time_d:         %f\n", time_d);
        fprintf(stderr, "dbg2       navlon:         %.10f\n", navlon);
        fprintf(stderr, "dbg2       navlat:         %.10f\n", navlat);
        fprintf(stderr, "dbg2       speed:          %f\n", speed);
        fprintf(stderr, "dbg2       heading:        %f\n", heading);
        fprintf(stderr, "dbg2       distance:       %f\n", distance);
        fprintf(stderr, "dbg2       beams_bath:     %d\n", beams_bath);
        fprintf(stderr, "dbg2       beams_amp:      %d\n", beams_amp);
        fprintf(stderr, "dbg2       pixels_ss:      %d\n", pixels_ss);
            }
    }

    /* close the swath file */
    status = mb_close(verbose, &imbio_ptr, error);
    if (nfp != NULL)
      fclose(nfp);
    if (output_open) {
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->beamflag, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_acrosstrack, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_alongtrack, error);
      status = mb_close(verbose, &ombio_ptr, error);
    }

    /* deallocate bin arrays if needed */
    if (bin_nbath != NULL) {
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_nbath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bathacrosstrack, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bathalongtrack, error);
    }

    /* get coverage masks for each section */
    if (file != NULL && !first) {
      /* loop over all sections */
      for (int k = 0; k < file->num_sections; k++) {
        /* set section data to be read */
        section = (struct mbna_section *)&file->sections[k];
        sprintf(opath, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, file->id, k);

        /* initialize reading the swath file */
        if ((status = mb_read_init(verbose, opath, 71, 1, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                                   &ombio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) !=
            MB_SUCCESS) {
          mb_error(verbose, *error, &error_message);
          fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
          fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", path);
        }

        /* allocate memory for data arrays */
        if (status == MB_SUCCESS) {
          beamflag = NULL;
          bath = NULL;
          amp = NULL;
          bathacrosstrack = NULL;
          bathalongtrack = NULL;
          ss = NULL;
          ssacrosstrack = NULL;
          ssalongtrack = NULL;
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char),
                                       (void **)&beamflag, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bath, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp,
                                       error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bathacrosstrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bathalongtrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss,
                                       error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                                       (void **)&ssacrosstrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                                       (void **)&ssalongtrack, error);

          /* if error initializing memory then don't read the file */
          if (*error != MB_ERROR_NO_ERROR) {
            mb_error(verbose, *error, &error_message);
            fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
          }
        }

        /* loop over reading data */
        dx1 = (section->lonmax - section->lonmin) / MBNA_MASK_DIM;
        dy1 = (section->latmax - section->latmin) / MBNA_MASK_DIM;
        while (*error <= MB_ERROR_NO_ERROR) {
          /* read a ping of data */
          status =
              mb_get_all(verbose, ombio_ptr, &ostore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                         &heading, &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag,
                         bath, amp, bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

          /* ignore minor errors */
          if (kind == MB_DATA_DATA && (*error == MB_ERROR_TIME_GAP || *error == MB_ERROR_OUT_BOUNDS ||
                                       *error == MB_ERROR_OUT_TIME || *error == MB_ERROR_SPEED_TOO_SMALL)) {
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
          }

          /* check for good bathymetry */
          if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR) {
            mb_coor_scale(verbose, navlat, &mtodeglon, &mtodeglat);
            headingx = sin(DTR * heading);
            headingy = cos(DTR * heading);
            for (int i = 0; i < beams_bath; i++) {
              if (mb_beam_ok(beamflag[i]) && bath[i] != 0.0) {
                lon =
                    navlon + headingy * mtodeglon * bathacrosstrack[i] + headingx * mtodeglon * bathalongtrack[i];
                lat =
                    navlat - headingx * mtodeglat * bathacrosstrack[i] + headingy * mtodeglat * bathalongtrack[i];
                ii1 = (lon - section->lonmin) / dx1;
                jj1 = (lat - section->latmin) / dy1;
                if (ii1 >= 0 && ii1 < MBNA_MASK_DIM && jj1 >= 0 && jj1 < MBNA_MASK_DIM) {
                  section->coverage[ii1 + jj1 * MBNA_MASK_DIM] = 1;
                }
              }
            }
          }
        }

        /* deallocate memory used for data arrays */
        status = mb_close(verbose, &ombio_ptr, error);
      }
    }
  }

  /* add info text */
  if (status == MB_SUCCESS && new_pings > 0) {
    sprintf(message, "Imported format %d file: %s\n > Read %d pings\n > Added %d sections %d crossings\n", iformat, path,
            new_pings, new_sections, new_crossings);
    mbnavadjust_info_add(verbose, project, message, true, error);
  }
  else {
    sprintf(message, "Unable to import format %d file: %s\n", iformat, path);
    mbnavadjust_info_add(verbose, project, message, true, error);
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_reimport_file(int verbose, struct mbna_project *project,
                                int ifile, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       ifile:            %d\n", ifile);
  }

  int status = MB_SUCCESS;
  char *error_message;
  mb_path ipath;
  int iformat;

  if (ifile <  0 || ifile >= project->num_files) {
    *error = MB_ERROR_BAD_DATA;
    status = MB_FAILURE;
  } else {

    /* look for raw file first, then look for processed file and use if available */
    struct stat file_status;
    int fstat = stat(project->files[ifile].path, &file_status);
    if (fstat == 0 && (file_status.st_mode & S_IFMT) != S_IFDIR) {
      int ofile_specified = MB_NO;
      mb_pr_get_ofile(verbose, project->files[ifile].path, &ofile_specified, ipath, error);
      if (ofile_specified == MB_YES) {
        fstat = stat(ipath, &file_status);
        if (fstat != 0 || (file_status.st_mode & S_IFMT) == S_IFDIR) {
          ofile_specified = MB_NO;
        }
      }
      if (ofile_specified == MB_NO) {
        strcpy(ipath, project->files[ifile].path);
      }
      iformat = project->files[ifile].format;
      *error = MB_ERROR_NO_ERROR;
    }
    else {
    *error = MB_ERROR_BAD_DATA;
    status = MB_FAILURE;
    }
  }

  struct stat file_status;
  char npath[STRING_MAX];
  char opath[STRING_MAX];

  /* MBIO control parameters */
  int pings;
  int lonflip;
  double bounds[4];
  int btime_i[7];
  int etime_i[7];
  double btime_d;
  double etime_d;
  double speedmin;
  double timegap;

  /* mbio read and write values */
  void *imbio_ptr = NULL;
  void *ombio_ptr = NULL;
  void *istore_ptr = NULL;
  void *ostore_ptr = NULL;
  int kind = MB_DATA_NONE;
  int time_i[7];
  double time_d;
  double navlon;
  double navlat;
  double speed;
  double heading;
  double distance;
  double altitude;
  double sonardepth;
  double draft;
  double roll;
  double pitch;
  double heave;
  int beams_bath = 0;
  int beams_amp = 0;
  int pixels_ss = 0;
  char comment[MB_COMMENT_MAXLINE];

  int sonartype = MB_TOPOGRAPHY_TYPE_UNKNOWN;
  int sensorhead = 0;
  int *bin_nbath = NULL;
  double *bin_bath = NULL;
  double *bin_bathacrosstrack = NULL;
  double *bin_bathalongtrack = NULL;
  int side;
  double port_time_d, stbd_time_d;
  double angle, dt, alongtrackdistance, xtrackavg, xtrackmax;
  int nxtrack;

  int obeams_bath, obeams_amp, opixels_ss;
  int nread;
  bool first;
  double headingx, headingy, mtodeglon, mtodeglat;
  double lon, lat;
  double navlon_old, navlat_old, sensordepth_old;
  FILE *nfp;
  struct mbna_file *file = NULL;
  struct mbna_section *section = NULL;
  struct mbna_file *cfile = NULL;
  struct mbna_section *csection = NULL;
  struct mbsys_ldeoih_struct *ostore = NULL;
  struct mb_io_struct *omb_io_ptr = NULL;
  double dx1, dy1;
  int mbp_heading_mode;
  double mbp_headingbias;
  int mbp_rollbias_mode;
  double mbp_rollbias;
  double mbp_rollbias_port;
  double mbp_rollbias_stbd;
  double depthmax, distmax, depthscale, distscale;
  int error_sensorhead = MB_ERROR_NO_ERROR;
  void *tptr;
  int ii1, jj1;

  char *root = (char *)strrchr(ipath, '/');
  if (root == NULL)
    root = ipath;
  char message[STRING_MAX];
  sprintf(message, "Re-importing format %d data from %s in %d sections\n", iformat, root, project->files[ifile].num_sections);
  fprintf(stderr, "%s\n", message);
  bool output_open = false;
  if (project->inversion_status == MBNA_INVERSION_CURRENT)
    project->inversion_status = MBNA_INVERSION_OLD;
  project->grid_status = MBNA_GRID_OLD;
  int new_sections = 0;
  int new_pings = 0;
  int new_crossings = 0;
  int good_beams = 0;

  if (status == MB_SUCCESS) {
    /* initialize reading the swath file */
    if ((status = mb_read_init(verbose, ipath, iformat, pings, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                               &imbio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) != MB_SUCCESS) {
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
      fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", ipath);
    }
  }

  char *beamflag = NULL;
  double *bath = NULL;
  double *bathacrosstrack = NULL;
  double *bathalongtrack = NULL;
  double *amp = NULL;
  double *ss = NULL;
  double *ssacrosstrack = NULL;
  double *ssalongtrack = NULL;

  /* allocate memory for data arrays */
  if (status == MB_SUCCESS) {
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char), (void **)&beamflag, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bath, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp, error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bathacrosstrack,
                                 error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double), (void **)&bathalongtrack,
                                 error);
    if (*error == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss, error);
    if (*error == MB_ERROR_NO_ERROR)
      status =
          mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssacrosstrack, error);
    if (*error == MB_ERROR_NO_ERROR)
      status =
          mb_register_array(verbose, imbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ssalongtrack, error);
    if (*error == MB_ERROR_NO_ERROR)

    /* if error initializing memory then don't read the file */
    if (*error != MB_ERROR_NO_ERROR) {
      mb_error(verbose, *error, &error_message);
      fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
    }
  }

  /* open nav file */
  if (status == MB_SUCCESS) {
    sprintf(npath, "%s/nvs_%4.4d.mb166", project->datadir, ifile);
    if ((nfp = fopen(npath, "w")) == NULL) {
      status = MB_FAILURE;
      *error = MB_ERROR_OPEN_FAIL;
    }
  }

  /* read data */
  if (status == MB_SUCCESS) {
    nread = 0;
    bool new_section = false;
    int isection = -1;
    first = true;
    while (*error <= MB_ERROR_NO_ERROR) {
      /* read a ping of data */
      status = mb_get_all(verbose, imbio_ptr, &istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed, &heading,
                          &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag, bath, amp,
                          bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

      /* extract all nav values */
      if (*error == MB_ERROR_NO_ERROR && (kind == MB_DATA_NAV || kind == MB_DATA_DATA)) {
        status = mb_extract_nav(verbose, imbio_ptr, istore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                                &heading, &draft, &roll, &pitch, &heave, error);
      }

      /* ignore minor errors - use these data */
      if (kind == MB_DATA_DATA && (*error == MB_ERROR_TIME_GAP || *error == MB_ERROR_OUT_BOUNDS ||
                                   *error == MB_ERROR_OUT_TIME || *error == MB_ERROR_SPEED_TOO_SMALL)) {
        status = MB_SUCCESS;
        *error = MB_ERROR_NO_ERROR;
      }

      /* do not use data with null navigation or ludricous timestamp */
      if (kind == MB_DATA_DATA) {
        if (navlon == 0.0 && navlat == 0.0)
          *error = MB_ERROR_IGNORE;
        if (time_d <= 0.0 || time_i[0] < 1962 || time_i[0] > 3000)
          *error = MB_ERROR_IGNORE;
      }

            /* deal with survey data */
      if (kind == MB_DATA_DATA) {
        /* int status_sensorhead = */
        mb_sensorhead(verbose, imbio_ptr, istore_ptr, &sensorhead, &error_sensorhead);
        if (sonartype == MB_TOPOGRAPHY_TYPE_UNKNOWN) {
          status = mb_sonartype(verbose, imbio_ptr, istore_ptr, &sonartype, error);
        }

        /* if sonar is interferometric, bin the bathymetry */
        if (sonartype == MB_TOPOGRAPHY_TYPE_INTERFEROMETRIC) {
          /* allocate bin arrays if needed */
          if (bin_nbath == NULL) {
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(int),
                                (void **)&bin_nbath, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bath, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bathacrosstrack, error);
            status = mb_mallocd(verbose, __FILE__, __LINE__, project->bin_beams_bath * sizeof(double),
                                (void **)&bin_bathalongtrack, error);
            for (int i = 0; i < project->bin_beams_bath; i++) {
              bin_nbath[i] = 0;
              bin_bath[i] = 0.0;
              bin_bathacrosstrack[i] = 0.0;
              bin_bathalongtrack[i] = 0.0;
            }
          }

          /* figure out if this is a ping to one side or across the whole swath */
          xtrackavg = 0.0;
          xtrackmax = 0.0;
          nxtrack = 0;
          for (int i = 0; i < beams_bath; i++) {
            if (mb_beam_ok(beamflag[i])) {
              xtrackavg += bathacrosstrack[i];
              xtrackmax = MAX(xtrackmax, fabs(bathacrosstrack[i]));
              nxtrack++;
            }
          }
          if (nxtrack > 0) {
            xtrackavg /= nxtrack;
          }
          if (xtrackavg > 0.25 * xtrackmax) {
            side = SIDE_STBD;
            port_time_d = time_d;
          }
          else if (xtrackavg < -0.25 * xtrackmax) {
            side = SIDE_PORT;
            stbd_time_d = time_d;
          }
          else {
            side = SIDE_FULLSWATH;
            stbd_time_d = time_d;
          }

          /* if side = PORT or FULLSWATH then initialize bin arrays */
          if (side == SIDE_PORT || side == SIDE_FULLSWATH) {
            for (int i = 0; i < project->bin_beams_bath; i++) {
              bin_nbath[i] = 0;
              bin_bath[i] = 0.0;
              bin_bathacrosstrack[i] = 0.0;
              bin_bathalongtrack[i] = 0.0;
            }
          }

          /* bin the bathymetry */
          for (int i = 0; i < beams_bath; i++) {
            if (mb_beam_ok(beamflag[i])) {
              /* get apparent acrosstrack beam angle and bin accordingly */
              angle = RTD * atan(bathacrosstrack[i] / (bath[i] - sonardepth));
              const int j = (int)floor((angle + 0.5 * project->bin_swathwidth + 0.5 * project->bin_pseudobeamwidth) /
                             project->bin_pseudobeamwidth);
              /* fprintf(stderr,"i:%d bath:%f %f %f sonardepth:%f angle:%f j:%d\n",
              i,bath[i],bathacrosstrack[i],bathalongtrack[i],sonardepth,angle,j); */
              if (j >= 0 && j < project->bin_beams_bath) {
                bin_bath[j] += bath[i];
                bin_bathacrosstrack[j] += bathacrosstrack[i];
                bin_bathalongtrack[j] += bathalongtrack[i];
                bin_nbath[j]++;
              }
            }
          }

          /* if side = STBD or FULLSWATH calculate output bathymetry
              - add alongtrack offset to port data from previous ping */
          if (side == SIDE_STBD || side == SIDE_FULLSWATH) {
            dt = port_time_d - stbd_time_d;
            if (dt > 0.0 && dt < 0.5)
              alongtrackdistance = -(port_time_d - stbd_time_d) * speed / 3.6;
            else
              alongtrackdistance = 0.0;
            beams_bath = project->bin_beams_bath;
            for (int j = 0; j < project->bin_beams_bath; j++) {
              /* fprintf(stderr,"j:%d angle:%f n:%d bath:%f %f %f\n",
              j,j*project->bin_pseudobeamwidth - 0.5 *
              project->bin_swathwidth,bin_nbath[j],bin_bath[j],bin_bathacrosstrack[j],bin_bathalongtrack[j]); */
              if (bin_nbath[j] > 0) {
                bath[j] = bin_bath[j] / bin_nbath[j];
                bathacrosstrack[j] = bin_bathacrosstrack[j] / bin_nbath[j];
                bathalongtrack[j] = bin_bathalongtrack[j] / bin_nbath[j];
                beamflag[j] = MB_FLAG_NONE;
                if (bin_bathacrosstrack[j] < 0.0)
                  bathalongtrack[j] += alongtrackdistance;
              }
              else {
                beamflag[j] = MB_FLAG_NULL;
                bath[j] = 0.0;
                bathacrosstrack[j] = 0.0;
                bathalongtrack[j] = 0.0;
              }
            }
          }

          /* if side = PORT set nonfatal error so that bathymetry isn't output until
              the STBD data are read too */
          else if (side == SIDE_PORT) {
            *error = MB_ERROR_IGNORE;
          }
        }
      }

      /* deal with start of file */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && first) {

        first = false;
        new_section = true;
        file = &project->files[ifile];

        /* get bias values */
        mb_pr_get_heading(verbose, file->path, &mbp_heading_mode, &mbp_headingbias, error);
        mb_pr_get_rollbias(verbose, file->path, &mbp_rollbias_mode, &mbp_rollbias, &mbp_rollbias_port,
                           &mbp_rollbias_stbd, error);
        if (mbp_heading_mode == MBP_HEADING_OFFSET || mbp_heading_mode == MBP_HEADING_CALCOFFSET) {
          file->heading_bias_import = mbp_headingbias;
        }
        else {
          file->heading_bias_import = 0.0;
        }
        if (mbp_rollbias_mode == MBP_ROLLBIAS_SINGLE) {
          file->roll_bias_import = mbp_rollbias;
        }
        else if (mbp_rollbias_mode == MBP_ROLLBIAS_DOUBLE) {
          file->roll_bias_import = 0.5 * (mbp_rollbias_port + mbp_rollbias_stbd);
        }
        else {
          file->roll_bias_import = 0.0;
        }
      }

      /* check if new section needed */
      else if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && file != NULL
               && time_d > section->etime_d) {
        new_section = true;
      }

      /* deal with new section */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && new_section) {
        new_section = false;

        /* end old section */
        if (output_open) {
          /* close the swath file */
          status = mb_close(verbose, &ombio_ptr, error);
          output_open = false;
        }

        /* initialize new section */
        isection++;
        section = &file->sections[isection];
        section->num_beams = 0;
        for (int i = 0; i < MBNA_MASK_DIM * MBNA_MASK_DIM; i++)
          section->coverage[i] = 0;
        section->distance = 0.0;
        section->lonmin = navlon;
        section->lonmax = navlon;
        section->latmin = navlat;
        section->latmax = navlat;
        section->depthmin = 0.0;
        section->depthmax = 0.0;
        section->contoursuptodate = false;
        section->inversion_offset_x = 0.0;
        section->inversion_offset_y = 0.0;
        section->inversion_offset_x_m = 0.0;
        section->inversion_offset_y_m = 0.0;
        section->inversion_offset_z_m = 0.0;

        /* open output section file */
        sprintf(opath, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, file->id, isection);
        if ((status = mb_write_init(verbose, opath, 71, &ombio_ptr, &obeams_bath, &obeams_amp, &opixels_ss,
                                    error)) != MB_SUCCESS) {
          mb_error(verbose, *error, &error_message);
          fprintf(stderr, "\nMBIO Error returned from function <mb_write_init>:\n%s\n", error_message);
          fprintf(stderr, "\nSwath sonar File <%s> not initialized for writing\n", opath);
        }
        else {
          omb_io_ptr = (struct mb_io_struct *)ombio_ptr;
          ostore_ptr = omb_io_ptr->store_data;
          ostore = (struct mbsys_ldeoih_struct *)ostore_ptr;
          ostore->kind = MB_DATA_DATA;
          ostore->beams_bath = obeams_bath;
          ostore->beams_amp = 0;
          ostore->pixels_ss = 0;
                    ostore->sensorhead = sensorhead;
                    ostore->topo_type = sonartype;
          output_open = true;
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(char), (void **)&ostore->beamflag,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double), (void **)&ostore->bath,
                              error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double),
                              (void **)&ostore->bath_acrosstrack, error);
          status = mb_mallocd(verbose, __FILE__, __LINE__, obeams_bath * sizeof(double),
                              (void **)&ostore->bath_alongtrack, error);

          /* if error initializing memory then don't write the file */
          if (*error != MB_ERROR_NO_ERROR) {
            mb_error(verbose, *error, &error_message);
            fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->beamflag, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_acrosstrack, error);
            status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_alongtrack, error);
            status = mb_close(verbose, &ombio_ptr, error);
            output_open = false;
          }
        }
      }

      /* update section distance for each data ping */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR && section->num_pings > 1)
        section->distance += distance;

      /* handle good bathymetry */
      if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR) {
        /* get statistics */
        mb_coor_scale(verbose, navlat, &mtodeglon, &mtodeglat);
        headingx = sin(DTR * heading);
        headingy = cos(DTR * heading);
        navlon_old = navlon;
        navlat_old = navlat;
        sensordepth_old = draft - heave;
        section->num_pings++;
        file->num_pings++;
        new_pings++;
        if (section->distance >= section->num_snav * project->section_length / (MBNA_SNAV_NUM - 1)) {
          section->snav_id[section->num_snav] = section->num_pings - 1;
          section->snav_num_ties[section->num_snav] = 0;
          section->snav_distance[section->num_snav] = section->distance;
          section->snav_time_d[section->num_snav] = time_d;
          section->snav_lon[section->num_snav] = navlon;
          section->snav_lat[section->num_snav] = navlat;
          section->snav_sensordepth[section->num_snav] = draft - heave;;
          section->snav_lon_offset[section->num_snav] = 0.0;
          section->snav_lat_offset[section->num_snav] = 0.0;
          section->snav_z_offset[section->num_snav] = 0.0;
          section->num_snav++;
          file->num_snavs++;
          project->num_snavs++;
        }
        for (int i = 0; i < beams_bath; i++) {
          if (mb_beam_ok(beamflag[i]) && bath[i] != 0.0) {
            good_beams++;
            project->num_beams++;
            file->num_beams++;
            section->num_beams++;
            lon = navlon + headingy * mtodeglon * bathacrosstrack[i] + headingx * mtodeglon * bathalongtrack[i];
            lat = navlat - headingx * mtodeglat * bathacrosstrack[i] + headingy * mtodeglat * bathalongtrack[i];
            if (lon != 0.0)
              section->lonmin = MIN(section->lonmin, lon);
            if (lon != 0.0)
              section->lonmax = MAX(section->lonmax, lon);
            if (lat != 0.0)
              section->latmin = MIN(section->latmin, lat);
            if (lat != 0.0)
              section->latmax = MAX(section->latmax, lat);
            if (section->depthmin == 0.0)
              section->depthmin = bath[i];
            else
              section->depthmin = MIN(section->depthmin, bath[i]);
            if (section->depthmin == 0.0)
              section->depthmax = bath[i];
            else
              section->depthmax = MAX(section->depthmax, bath[i]);
          }
          else {
            beamflag[i] = MB_FLAG_NULL;
            bath[i] = 0.0;
            bathacrosstrack[i] = 0.0;
            bathalongtrack[i] = 0.0;
          }
        }

        /* write out bath data only to format 71 file */
        if (output_open) {

          /* get depth and distance scaling */
          depthmax = 0.0;
          distmax = 0.0;
          for (int i = 0; i < beams_bath; i++) {
            depthmax = MAX(depthmax, fabs(bath[i]));
            distmax = MAX(distmax, fabs(bathacrosstrack[i]));
            distmax = MAX(distmax, fabs(bathalongtrack[i]));
          }
          depthscale = MAX(0.001, depthmax / 32000);
          distscale = MAX(0.001, distmax / 32000);
          ostore->depth_scale = depthscale;
          ostore->distance_scale = distscale;
          ostore->sonardepth = draft - heave;
          ostore->roll = roll;
          ostore->pitch = pitch;
          ostore->heave = heave;

          /* write out data */
          status = mb_put_all(verbose, ombio_ptr, ostore_ptr,
                                        true, MB_DATA_DATA, time_i, time_d, navlon, navlat,
                              speed, heading, beams_bath, 0, 0,
                                        beamflag, bath, amp,
                                        bathacrosstrack, bathalongtrack,
                              ss, ssacrosstrack, ssalongtrack, comment, error);
        }

        /* write out nav data associated with bath to format 166 file */
        if (nfp != NULL) {
          fprintf(nfp, "%4.4d %2.2d %2.2d %2.2d %2.2d %2.2d.%6.6d %16.6f %.10f %.10f %.2f %.2f %.2f %.2f %.2f %.2f\r\n",
                  time_i[0], time_i[1], time_i[2], time_i[3], time_i[4], time_i[5], time_i[6], time_d, navlon, navlat,
                  heading, speed, draft, roll, pitch, heave);
        }
      }

      /* increment counters */
      if (*error == MB_ERROR_NO_ERROR)
        nread++;

      /* print debug statements */
      if (verbose >= 2) {
        fprintf(stderr, "\ndbg2  Ping read in program <%s>\n", program_name);
        fprintf(stderr, "dbg2       kind:           %d\n", kind);
        fprintf(stderr, "dbg2       error:          %d\n", *error);
        fprintf(stderr, "dbg2       status:         %d\n", status);
      }
      if (verbose >= 2 && kind == MB_DATA_COMMENT) {
        fprintf(stderr, "dbg2       comment:        %s\n", comment);
      }
      if (verbose >= 2 && error <= 0 && kind == MB_DATA_DATA) {
        fprintf(stderr, "dbg2       time_i:         %4d/%2d/%2d %2.2d:%2.2d:%2.2d.%6.6d\n", time_i[0], time_i[1],
                time_i[2], time_i[3], time_i[4], time_i[5], time_i[6]);
        fprintf(stderr, "dbg2       time_d:         %f\n", time_d);
        fprintf(stderr, "dbg2       navlon:         %.10f\n", navlon);
        fprintf(stderr, "dbg2       navlat:         %.10f\n", navlat);
        fprintf(stderr, "dbg2       speed:          %f\n", speed);
        fprintf(stderr, "dbg2       heading:        %f\n", heading);
        fprintf(stderr, "dbg2       distance:       %f\n", distance);
        fprintf(stderr, "dbg2       beams_bath:     %d\n", beams_bath);
        fprintf(stderr, "dbg2       beams_amp:      %d\n", beams_amp);
        fprintf(stderr, "dbg2       pixels_ss:      %d\n", pixels_ss);
            }
    }

    /* close the swath file */
    status = mb_close(verbose, &imbio_ptr, error);
    if (nfp != NULL)
      fclose(nfp);
    if (output_open) {
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->beamflag, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_acrosstrack, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&ostore->bath_alongtrack, error);
      status = mb_close(verbose, &ombio_ptr, error);
    }

    /* deallocate bin arrays if needed */
    if (bin_nbath != NULL) {
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_nbath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bath, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bathacrosstrack, error);
      status = mb_freed(verbose, __FILE__, __LINE__, (void **)&bin_bathalongtrack, error);
    }

    /* get coverage masks for each section */
    if (file != NULL && !first) {
      /* loop over all sections */
      for (int k = 0; k < file->num_sections; k++) {
        /* set section data to be read */
        section = (struct mbna_section *)&file->sections[k];
        sprintf(opath, "%s/nvs_%4.4d_%4.4d.mb71", project->datadir, file->id, k);

        /* initialize reading the swath file */
        if ((status = mb_read_init(verbose, opath, 71, 1, lonflip, bounds, btime_i, etime_i, speedmin, timegap,
                                   &ombio_ptr, &btime_d, &etime_d, &beams_bath, &beams_amp, &pixels_ss, error)) !=
            MB_SUCCESS) {
          mb_error(verbose, *error, &error_message);
          fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", error_message);
          fprintf(stderr, "\nSwath sonar File <%s> not initialized for reading\n", opath);
        }

        /* allocate memory for data arrays */
        if (status == MB_SUCCESS) {
          beamflag = NULL;
          bath = NULL;
          amp = NULL;
          bathacrosstrack = NULL;
          bathalongtrack = NULL;
          ss = NULL;
          ssacrosstrack = NULL;
          ssalongtrack = NULL;
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char),
                                       (void **)&beamflag, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bath, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double), (void **)&amp,
                                       error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bathacrosstrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                                       (void **)&bathalongtrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double), (void **)&ss,
                                       error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                                       (void **)&ssacrosstrack, error);
          if (*error == MB_ERROR_NO_ERROR)
            status = mb_register_array(verbose, ombio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                                       (void **)&ssalongtrack, error);

          /* if error initializing memory then don't read the file */
          if (*error != MB_ERROR_NO_ERROR) {
            mb_error(verbose, *error, &error_message);
            fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", error_message);
          }
        }

        /* loop over reading data */
        dx1 = (section->lonmax - section->lonmin) / MBNA_MASK_DIM;
        dy1 = (section->latmax - section->latmin) / MBNA_MASK_DIM;
        while (*error <= MB_ERROR_NO_ERROR) {
          /* read a ping of data */
          status =
              mb_get_all(verbose, ombio_ptr, &ostore_ptr, &kind, time_i, &time_d, &navlon, &navlat, &speed,
                         &heading, &distance, &altitude, &sonardepth, &beams_bath, &beams_amp, &pixels_ss, beamflag,
                         bath, amp, bathacrosstrack, bathalongtrack, ss, ssacrosstrack, ssalongtrack, comment, error);

          /* ignore minor errors */
          if (kind == MB_DATA_DATA && (*error == MB_ERROR_TIME_GAP || *error == MB_ERROR_OUT_BOUNDS ||
                                       *error == MB_ERROR_OUT_TIME || *error == MB_ERROR_SPEED_TOO_SMALL)) {
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
          }

          /* check for good bathymetry */
          if (kind == MB_DATA_DATA && *error == MB_ERROR_NO_ERROR) {
            mb_coor_scale(verbose, navlat, &mtodeglon, &mtodeglat);
            headingx = sin(DTR * heading);
            headingy = cos(DTR * heading);
            for (int i = 0; i < beams_bath; i++) {
              if (mb_beam_ok(beamflag[i]) && bath[i] != 0.0) {
                lon =
                    navlon + headingy * mtodeglon * bathacrosstrack[i] + headingx * mtodeglon * bathalongtrack[i];
                lat =
                    navlat - headingx * mtodeglat * bathacrosstrack[i] + headingy * mtodeglat * bathalongtrack[i];
                ii1 = (lon - section->lonmin) / dx1;
                jj1 = (lat - section->latmin) / dy1;
                if (ii1 >= 0 && ii1 < MBNA_MASK_DIM && jj1 >= 0 && jj1 < MBNA_MASK_DIM) {
                  section->coverage[ii1 + jj1 * MBNA_MASK_DIM] = 1;
                }
              }
            }
          }
        }

        /* deallocate memory used for data arrays */
        status = mb_close(verbose, &ombio_ptr, error);
      }
    }
  }

  /* add info text */
  if (status == MB_SUCCESS && new_pings > 0) {
    sprintf(message, "Imported format %d file: %s\n > Read %d pings\n > Added %d sections %d crossings\n", iformat, ipath,
            new_pings, new_sections, new_crossings);
    mbnavadjust_info_add(verbose, project, message, true, error);
  }
  else {
    sprintf(message, "Unable to import format %d file: %s\n", iformat, ipath);
    mbnavadjust_info_add(verbose, project, message, true, error);
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_import_reference(int verbose, struct mbna_project *project, char *path, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       path:             %s\n", path);
  }

  // TODO(schwehr): Was there supposed to be some actual work?

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_findcrossings(int verbose, struct mbna_project *project, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
  }

  int status = MB_SUCCESS;

  /* loop over files looking for new crossings */
  if (project->open && project->num_files > 0) {
    /* look for new crossings through all files */
    for (int ifile = 0; ifile < project->num_files; ifile++) {
      status = mbnavadjust_findcrossingsfile(verbose, project, ifile, error);
    }

    /* resort crossings */
    if (project->num_crossings > 1)
      mb_mergesort((void *)project->crossings, (size_t)project->num_crossings, sizeof(struct mbna_crossing),
                   mbnavadjust_crossing_compare);

    /* recalculate overlap fractions, true crossings, good crossing statistics */
    project->num_crossings_analyzed = 0;
    project->num_goodcrossings = 0;
    project->num_truecrossings = 0;
    project->num_truecrossings_analyzed = 0;
    for (int icrossing = 0; icrossing < project->num_crossings; icrossing++) {
      struct mbna_crossing *crossing = &(project->crossings[icrossing]);

      /* recalculate crossing overlap */
      mbnavadjust_crossing_overlap(verbose, project, icrossing, error);
      if (crossing->overlap >= 25)
        project->num_goodcrossings++;

      /* check if this is a true crossing */
      if (mbnavadjust_sections_intersect(verbose, project, icrossing, error)) {
        crossing->truecrossing = true;
        project->num_truecrossings++;
        if (crossing->status != MBNA_CROSSING_STATUS_NONE)
          project->num_truecrossings_analyzed++;
      }
      else
        crossing->truecrossing = false;
      if (crossing->status != MBNA_CROSSING_STATUS_NONE)
        project->num_crossings_analyzed++;

      /* reset crossing ids in each tie structure */
      for (int itie = 0; itie < crossing->num_ties; itie++) {
        struct mbna_tie *tie = &crossing->ties[itie];
        tie->icrossing = icrossing;
        tie->itie = itie;
      }
    }

    /* write updated project */
    mbnavadjust_write_project(verbose, project, error);
    project->save_count = 0;
    project->modelplot_uptodate = false;
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_findcrossingsfile(int verbose, struct mbna_project *project, int ifile, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       ifile:            %d\n", ifile);
  }

  int status = MB_SUCCESS;

  /* loop over files and sections, comparing sections from project->files[ifile] with all previous sections */
  if (project->open && project->num_files > 0) {
    struct mbna_file *file2 = &(project->files[ifile]);

    /* loop over all sections */
    for (int isection = 0; isection < file2->num_sections; isection++) {
      // double cell1lonmin, cell1lonmax, cell1latmin, cell1latmax;
      // double cell2lonmin, cell2lonmax, cell2latmin, cell2latmax;
      struct mbna_section *section2 = &(file2->sections[isection]);
      const double lonoffset2 = section2->snav_lon_offset[section2->num_snav / 2];
      const double latoffset2 = section2->snav_lat_offset[section2->num_snav / 2];

      /* get coverage mask adjusted for most recent inversion solution */
      const double lonmin2 = section2->lonmin + lonoffset2;
      const double lonmax2 = section2->lonmax + lonoffset2;
      const double latmin2 = section2->latmin + latoffset2;
      const double latmax2 = section2->latmax + latoffset2;
      const double dx2 = (section2->lonmax - section2->lonmin) / (MBNA_MASK_DIM - 1);
      const double dy2 = (section2->latmax - section2->latmin) / (MBNA_MASK_DIM - 1);

      /* now loop over all previous sections looking for crossings */
      for (int jfile = 0; jfile <= ifile; jfile++) {
        struct mbna_file *file1 = &(project->files[jfile]);
        const int jsectionmax = jfile < ifile ? file1->num_sections : isection;
        for (int jsection = 0; jsection < jsectionmax; jsection++) {
          struct mbna_section *section1 = &(file1->sections[jsection]);
          const double lonoffset1 = section1->snav_lon_offset[section1->num_snav / 2];
          const double latoffset1 = section1->snav_lat_offset[section1->num_snav / 2];

          /* get coverage mask adjusted for most recent inversion solution */
          const double lonmin1 = section1->lonmin + lonoffset1;
          const double lonmax1 = section1->lonmax + lonoffset1;
          const double latmin1 = section1->latmin + latoffset1;
          const double latmax1 = section1->latmax + latoffset1;
          const double dx1 = (section1->lonmax - section1->lonmin) / (MBNA_MASK_DIM - 1);
          const double dy1 = (section1->latmax - section1->latmin) / (MBNA_MASK_DIM - 1);

          /* check if there is overlap given the current navigation model */
          int overlap = 0;
          bool disqualify = false;
          if (jfile == ifile && jsection == isection - 1 && section2->continuity)
            disqualify = true;
          else if (jfile == ifile - 1 && jsection == file1->num_sections - 1 && isection == 0 &&
                   section2->continuity)
            disqualify = true;
          else if (!(lonmin2 < lonmax1 && lonmax2 > lonmin1 && latmin2 < latmax1 && latmax2 > latmin1)) {
            disqualify = true;
          } else {
            /* loop over the coverage mask cells looking for overlap */
            for (int ii2 = 0; ii2 < MBNA_MASK_DIM && overlap == 0; ii2++)
              for (int jj2 = 0; jj2 < MBNA_MASK_DIM && overlap == 0; jj2++) {
                const int kk2 = ii2 + jj2 * MBNA_MASK_DIM;
                if (section2->coverage[kk2] == 1) {
                  const double cell2lonmin = lonmin2 + ii2 * dx2;
                  const double cell2lonmax = lonmin2 + (ii2 + 1) * dx2;
                  const double cell2latmin = latmin2 + jj2 * dy2;
                  const double cell2latmax = latmin2 + (jj2 + 1) * dy2;

                  for (int ii1 = 0; ii1 < MBNA_MASK_DIM && overlap == 0; ii1++) {
                    for (int jj1 = 0; jj1 < MBNA_MASK_DIM && overlap == 0; jj1++) {
                      const int kk1 = ii1 + jj1 * MBNA_MASK_DIM;
                      if (section1->coverage[kk1] == 1) {
                        const double cell1lonmin = lonmin1 + ii1 * dx1;
                        const double cell1lonmax = lonmin1 + (ii1 + 1) * dx1;
                        const double cell1latmin = latmin1 + jj1 * dy2;
                        const double cell1latmax = latmin1 + (jj1 + 1) * dy1;

                        /* check if these two cells overlap */
                        if (cell2lonmin < cell1lonmax && cell2lonmax > cell1lonmin &&
                            cell2latmin < cell1latmax && cell2latmax > cell1latmin) {
                          overlap++;
                        }
                      }
                    }
                  }
                }
              }
          }

          /* if not disqualified and overlap found, then this is a crossing */
          /* check to see if the crossing exists, if not add it */
          if (!disqualify && overlap > 0) {
            bool found = false;
            for (int icrossing = 0; icrossing < project->num_crossings && !found; icrossing++) {
              struct mbna_crossing *crossing = &(project->crossings[icrossing]);
              if (crossing->file_id_2 == ifile && crossing->file_id_1 == jfile && crossing->section_2 == isection &&
                  crossing->section_1 == jsection) {
                found = true;
              }
              else if (crossing->file_id_1 == ifile && crossing->file_id_2 == jfile &&
                       crossing->section_1 == isection && crossing->section_2 == jsection) {
                found = true;
              }
            }
            if (!found) {
              /* allocate mbna_crossing array if needed */
              if (project->num_crossings_alloc <= project->num_crossings) {
                project->crossings = (struct mbna_crossing *)realloc(
                    project->crossings, sizeof(struct mbna_crossing) * (project->num_crossings_alloc + ALLOC_NUM));
                if (project->crossings != NULL)
                  project->num_crossings_alloc += ALLOC_NUM;
                else {
                  status = MB_FAILURE;
                  *error = MB_ERROR_MEMORY_FAIL;
                }
              }

              /* add crossing to list */
              struct mbna_crossing *crossing = (struct mbna_crossing *)&project->crossings[project->num_crossings];
              crossing->status = MBNA_CROSSING_STATUS_NONE;
              crossing->truecrossing = false;
              crossing->overlap = 0;
              crossing->file_id_1 = file1->id;
              crossing->section_1 = jsection;
              crossing->file_id_2 = file2->id;
              crossing->section_2 = isection;
              crossing->num_ties = 0;
              project->num_crossings++;

              fprintf(stderr, "added crossing: %d  %4d %4d   %4d %4d\n", project->num_crossings - 1,
                      crossing->file_id_1, crossing->section_1, crossing->file_id_2, crossing->section_2);
            }
            /*else
            fprintf(stderr,"no new crossing:    %4d %4d   %4d %4d   duplicate\n",
            file2->id,isection,
            file1->id,jsection);*/
          }
          /*else
          fprintf(stderr,"disqualified:       %4d %4d   %4d %4d   disqualify:%d overlaptxt list:%d\n",
          file2->id,isection,
          file1->id,jsection, disqualify, overlap);*/
        }
      }
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_addcrossing(int verbose, struct mbna_project *project, int ifile1, int isection1, int ifile2, int isection2, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       ifile1:           %d\n", ifile1);
    fprintf(stderr, "dbg2       isection1:        %d\n", isection1);
    fprintf(stderr, "dbg2       ifile2:           %d\n", ifile2);
    fprintf(stderr, "dbg2       isection2:        %d\n", isection2);
  }

  /* check that file and section id's are reasonable */
  bool disqualify = false;
  if (ifile1 == ifile2 && isection1 == isection2)
    disqualify = true;
  if (!disqualify && (ifile1 < 0 || ifile1 >= project->num_files || ifile2 < 0 || ifile2 >= project->num_files))
    disqualify = true;
  if (!disqualify && (isection1 < 0 || isection1 >= project->files[ifile1].num_sections || isection2 < 0 ||
                                isection2 >= project->files[ifile2].num_sections))
    disqualify = true;

  /* check that this crossing does not already exist */
  if (!disqualify) {
    for (int icrossing = 0; icrossing < project->num_crossings && !disqualify; icrossing++) {
      struct mbna_crossing *crossing = (struct mbna_crossing *)&project->crossings[icrossing];
      if ((ifile1 == crossing->file_id_1 && isection1 == crossing->section_1 && ifile2 == crossing->file_id_2 &&
           isection2 == crossing->section_2) ||
          (ifile1 == crossing->file_id_1 && isection1 == crossing->section_1 && ifile2 == crossing->file_id_2 &&
           isection2 == crossing->section_2))
        disqualify = true;
    }
  }

  int status = MB_SUCCESS;

  /* if crossing ok and not yet defined then create it */
  if (!disqualify) {
    /* allocate mbna_crossing array if needed */
    if (project->num_crossings_alloc <= project->num_crossings) {
      project->crossings = (struct mbna_crossing *)realloc(project->crossings, sizeof(struct mbna_crossing) *
                                                                                 (project->num_crossings_alloc + ALLOC_NUM));
      if (project->crossings != NULL)
        project->num_crossings_alloc += ALLOC_NUM;
      else {
        status = MB_FAILURE;
        *error = MB_ERROR_MEMORY_FAIL;
      }
    }

    if (status == MB_SUCCESS) {
      /* add crossing to list */
      struct mbna_crossing *crossing = (struct mbna_crossing *)&project->crossings[project->num_crossings];
      crossing->status = MBNA_CROSSING_STATUS_NONE;
      crossing->truecrossing = false;
      crossing->overlap = 0;
      if (ifile1 < ifile2 || (ifile1 == ifile2 && isection1 < isection2)) {
        crossing->file_id_1 = ifile1;
        crossing->section_1 = isection1;
        crossing->file_id_2 = ifile2;
        crossing->section_2 = isection2;
      }
      else {
        crossing->file_id_1 = ifile2;
        crossing->section_1 = isection2;
        crossing->file_id_2 = ifile1;
        crossing->section_2 = isection1;
      }
      crossing->num_ties = 0;
      project->num_crossings++;

      fprintf(stderr, "added crossing: %d  %4d %4d   %4d %4d\n", project->num_crossings - 1, crossing->file_id_1,
              crossing->section_1, crossing->file_id_2, crossing->section_2);

      project->modelplot_uptodate = false;
    }
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int mbnavadjust_bin_bathymetry(int verbose, struct mbna_project *project,
                               double altitude, int beams_bath, char *beamflag, double *bath, double *bathacrosstrack,
                               double *bathalongtrack, int bin_beams_bath, double bin_pseudobeamwidth,
                               double bin_swathwidth, char *bin_beamflag, double *bin_bath, double *bin_bathacrosstrack,
                               double *bin_bathalongtrack, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:               %d\n", verbose);
    fprintf(stderr, "dbg2       project:               %p\n", project);
    fprintf(stderr, "dbg2       altitude:              %f\n", altitude);
    fprintf(stderr, "dbg2       beams_bath:            %d\n", beams_bath);
    for (int i = 0; i < beams_bath; i++)
      fprintf(stderr, "dbg2        beam[%d]: %f %f %f %d\n", i, bath[i], bathacrosstrack[i],
              bathalongtrack[i], beamflag[i]);
    fprintf(stderr, "dbg2       bin_beams_bath:        %d\n", bin_beams_bath);
    fprintf(stderr, "dbg2       bin_pseudobeamwidth:   %f\n", bin_pseudobeamwidth);
    fprintf(stderr, "dbg2       bin_swathwidth:        %f\n", bin_swathwidth);
  }

  const int status = MB_SUCCESS;

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    for (int i = 0; i < project->bin_beams_bath; i++)
      fprintf(stderr, "dbg2     beam[%d]: %f %f %f %d\n", i, bin_bath[i], bin_bathacrosstrack[i],
              bin_bathalongtrack[i], bin_beamflag[i]);
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
bool mbnavadjust_sections_intersect(int verbose, struct mbna_project *project, int crossing_id, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       crossing_id:  %d\n", crossing_id);
  }

  /* get crossing */
  struct mbna_crossing *crossing = (struct mbna_crossing *)&project->crossings[crossing_id];

  /* get section endpoints */
  struct mbna_file *file = &project->files[crossing->file_id_1];
  struct mbna_section *section = &file->sections[crossing->section_1];
  const double xa1 = section->snav_lon[0] + section->snav_lon_offset[0];
  const double ya1 = section->snav_lat[0] + section->snav_lat_offset[0];
  const double xa2 = section->snav_lon[section->num_snav - 1] + section->snav_lon_offset[section->num_snav - 1];
  const double ya2 = section->snav_lat[section->num_snav - 1] + section->snav_lat_offset[section->num_snav - 1];
  file = &project->files[crossing->file_id_2];
  section = &file->sections[crossing->section_2];
  const double xb1 = section->snav_lon[0] + section->snav_lon_offset[0];
  const double yb1 = section->snav_lat[0] + section->snav_lat_offset[0];
  const double xb2 = section->snav_lon[section->num_snav - 1] + section->snav_lon_offset[section->num_snav - 1];
  const double yb2 = section->snav_lat[section->num_snav - 1] + section->snav_lat_offset[section->num_snav - 1];

  /* check for parallel sections */
  const double dxa = xa2 - xa1;
  const double dya = ya2 - ya1;
  const double dxb = xb2 - xb1;
  const double dyb = yb2 - yb1;
  bool answer = false;
  if ((dxb * dya - dyb * dxa) != 0.0) {
    /* check for crossing sections */
    const double s = (dxa * (yb1 - ya1) + dya * (xa1 - xb1)) / (dxb * dya - dyb * dxa);
    const double t = (dxb * (ya1 - yb1) + dyb * (xb1 - xa1)) / (dyb * dxa - dxb * dya);
    answer = s >= 0.0 && s <= 1.0 && t >= 0.0 && t <= 1.0;
  }

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2       answer:      %d\n", answer);
  }

  return (answer);
}
/*--------------------------------------------------------------------*/
int mbnavadjust_crossing_compare(const void *a, const void *b) {
  struct mbna_crossing *aa = (struct mbna_crossing *)a;
  struct mbna_crossing *bb = (struct mbna_crossing *)b;

  const int a1id = aa->file_id_1 * 1000 + aa->section_1;
  const int a2id = aa->file_id_2 * 1000 + aa->section_2;
  const int aid = a1id > a2id ? a1id : a2id;

  const int b1id = bb->file_id_1 * 1000 + bb->section_1;
  const int b2id = bb->file_id_2 * 1000 + bb->section_2;
  const int bid =  b1id > b2id ? b1id : b2id;

  if (aid > bid)
    return (1);
  else if (aid < bid)
    return (-1);
  else if (a1id > b1id)
    return (1);
  else if (a1id < b1id)
    return (-1);
  else if (a2id > b2id)
    return (1);
  else if (a2id < b2id)
    return (-1);
  else
    return (0);
}
/*--------------------------------------------------------------------*/
/*   function mbnavadjust_tie_compare compares ties according to the
    misfit magnitude between the tie offset and the inversion model */
int mbnavadjust_tie_compare(const void *a, const void *b) {

  struct mbna_tie *tie_a = *((struct mbna_tie **)a);
  struct mbna_tie *tie_b = *((struct mbna_tie **)b);

  /* return according to the misfit magnitude */
  if (tie_a->inversion_status != MBNA_INVERSION_NONE
      && tie_b->inversion_status != MBNA_INVERSION_NONE) {
    if (tie_a->sigma_m > tie_b->sigma_m) {
      return(1);
    } else {
      return(-1);
    }
  } else if (tie_a->inversion_status != MBNA_INVERSION_NONE) {
    return(1);
  } else {
    return(-1);
  }
}

/*--------------------------------------------------------------------*/

int mbnavadjust_info_add(int verbose, struct mbna_project *project, char *info, bool timetag, int *error) {
  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2       verbose:          %d\n", verbose);
    fprintf(stderr, "dbg2       project:          %p\n", project);
    fprintf(stderr, "dbg2       info:             %s\n", info);
    fprintf(stderr, "dbg2       timetag:          %d\n", timetag);
  }

	/* add text */
	if (project->logfp != NULL)
		fputs(info, project->logfp);
	if (verbose > 0)
		fputs(info, stderr);

	/* put time tag in if requested */
	if (timetag) {
    char user[256], host[256], date[32];
	  int error = MB_ERROR_NO_ERROR;
    int status = mb_user_host_date(verbose, user, host, date, &error);
		char tag[STRING_MAX];
		sprintf(tag, " > User <%s> on cpu <%s> at <%s>\n", user, host, date);
		if (project->logfp != NULL)
			fputs(tag, project->logfp);
		if (verbose > 0)
			fputs(tag, stderr);
	}

  if (verbose >= 2) {
    fprintf(stderr, "\ndbg2  MBnavadjust function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", *error);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", MB_SUCCESS);
  }

	return (MB_SUCCESS);
}

/*--------------------------------------------------------------------*/
