/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit vmod.vcc and run vmod.py instead
 */

struct sess;
struct VCL_conf;
struct vmod_priv;

const char * vmod_toupper(struct sess *, struct vmod_priv *, const char *, ...);
const char * vmod_tolower(struct sess *, struct vmod_priv *, const char *, ...);
void vmod_set_ip_tos(struct sess *, int);
double vmod_random(struct sess *, double, double);
void vmod_log(struct sess *, const char *, ...);
void vmod_syslog(struct sess *, int, const char *, ...);
const char * vmod_fileread(struct sess *, struct vmod_priv *, const char *);
const char * vmod_author(struct sess *, const char *);
double vmod_duration(struct sess *, const char *, double);
int vmod_integer(struct sess *, const char *, int);
void vmod_collect(struct sess *, enum gethdr_e, const char *);
int init_function(struct vmod_priv *, const struct VCL_conf *);
extern const void * const Vmod_Id;
