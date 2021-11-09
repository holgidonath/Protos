#ifndef _MAIN_H_
#define _MAIN_H_

#ifndef VERSION
#define VERSION  "0.0.1"
#endif


/* Read and set cmd line options POSIX style */
void parseOptions(int argc, char **argv, struct opt *opt);

/* Display help */
void help();

/* Display usage */
void usage();

/* Display version related info */
void version();

#endif