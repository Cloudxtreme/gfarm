/*
 * $Id$
 */

#include <gfarm/gfarm_config.h>

/*
 * basic types
 */
typedef unsigned char	gfarm_uint8_t;
typedef unsigned short	gfarm_uint16_t;
typedef unsigned int	gfarm_uint32_t;
typedef char		gfarm_int8_t;
typedef short		gfarm_int16_t;
typedef int		gfarm_int32_t;

typedef gfarm_int64_t gfarm_pid_t; /* XXX - need better place */

/*
 * username handling
 *
 * XXX these functions do not care about a metadata server.
 * do not use.
 */

/* the return value of the following functions should be free(3)ed */
gfarm_error_t gfarm_global_to_local_username(char *, char **);
gfarm_error_t gfarm_local_to_global_username(char *, char **);
gfarm_error_t gfarm_global_to_local_groupname(char *, char **);
gfarm_error_t gfarm_local_to_global_groupname(char *, char **);

/*
 * the return value of the following gfarm_get_*() funtions should not be
 * trusted (maybe forged) on client side, because any user can forge
 * the name by simply setting $USER.
 */
gfarm_error_t gfarm_set_global_username(char *);
gfarm_error_t gfarm_set_local_username(char *);
gfarm_error_t gfarm_set_local_homedir(char *);
char *gfarm_get_global_username(void);
char *gfarm_get_local_username(void);
char *gfarm_get_local_homedir(void);
gfarm_error_t gfarm_set_local_user_for_this_local_account(void);
gfarm_error_t gfarm_set_global_user_for_this_local_account(void);

/*
 * gfarm.conf
 */

/* the following functions are for client, */
/* server/daemon process shouldn't call follows: */
gfarm_error_t gfarm_initialize(int *, char ***);
gfarm_error_t gfarm_terminate(void);
gfarm_error_t gfarm_config_read(void);

/* the following function is for server. */
gfarm_error_t gfarm_server_initialize(void);
gfarm_error_t gfarm_server_terminate(void);
gfarm_error_t gfarm_server_config_read(void);
void gfarm_config_set_filename(char *);

int gfarm_xattr_caching(const char *);

/*
 * GFarm URL and pathname handling
 */

gfarm_error_t gfarm_canonical_path(const char *, char **);
gfarm_error_t gfarm_canonical_path_for_creation(const char *, char **);
gfarm_error_t gfarm_url_make_path(const char *, char **);
gfarm_error_t gfarm_url_make_path_for_creation(const char *, char **);
int gfarm_is_url(const char *);
gfarm_error_t gfarm_path_canonical_to_url(const char *, char **);

const char *gfarm_url_prefix_skip(const char *);
gfarm_error_t gfarm_url_prefix_add(const char *);
const char *gfarm_path_dir_skip(const char *);

extern char GFARM_URL_PREFIX[];
#define GFARM_URL_PREFIX_LENGTH 6
extern const char GFARM_PATH_ROOT[];

/*
 * File System Node Scheduling
 * XXX - will be separated to <gfarm_schedule.h>?
 */
struct gfarm_host_sched_info;
gfarm_error_t gfarm_schedule_hosts_domain_all(const char *, const char *,
	int *, struct gfarm_host_sched_info **);
gfarm_error_t gfarm_schedule_hosts_domain_by_file(
	const char *, int, const char *,
	int *, struct gfarm_host_sched_info **);
void gfarm_host_sched_info_free(int, struct gfarm_host_sched_info *);

void gfarm_schedule_search_mode_use_loadavg(void);
gfarm_error_t gfarm_schedule_hosts(const char *,
	int, struct gfarm_host_sched_info *, int, char **, int *);
gfarm_error_t gfarm_schedule_hosts_to_write(const char *,
	int, struct gfarm_host_sched_info *, int, char **, int *);
gfarm_error_t gfarm_schedule_hosts_acyclic(const char *,
	int, struct gfarm_host_sched_info *, int *, char **, int *);
gfarm_error_t gfarm_schedule_hosts_acyclic_to_write(const char *,
	int, struct gfarm_host_sched_info *, int *, char **, int *);

/* for debugging */
void gfarm_schedule_cache_dump(void);

/*
 * helper functions for import
 */
gfarm_error_t gfarm_hostlist_read(char *, int *, char ***, int *);

/*
 * hostspec
 */
int gfarm_host_is_in_domain(const char *, const char *);

/*
 * host
 */
char *gfarm_host_get_self_name(void);

/*
 * Miscellaneous
 */
#define GFARM_INT32STRLEN 11	/* max strlen(sprintf(s, "%d", int32)) */
#define GFARM_INT64STRLEN 22	/* max strlen(sprintf(s, "%lld", int64)) */

#define GFARM_ARRAY_LENGTH(array)	(sizeof(array)/sizeof(array[0]))

#define GFARM_MALLOC(p)		((p) = malloc(sizeof(*(p))))
#define GFARM_CALLOC_ARRAY(p,n)	((p) = gfarm_calloc_array((n), sizeof(*(p))))
#define GFARM_MALLOC_ARRAY(p,n)	((p) = gfarm_malloc_array((n), sizeof(*(p))))
#define GFARM_REALLOC_ARRAY(d,s,n)	((d) = gfarm_realloc_array((s), (n), sizeof(*(d))))

void *gfarm_calloc_array(size_t, size_t);
void *gfarm_malloc_array(size_t, size_t);
void *gfarm_realloc_array(void *, size_t, size_t);

gfarm_error_t gfarm_fixedstrings_dup(int, char **, char **);
void gfarm_strings_free_deeply(int, char **);
int gfarm_strarray_length(char **);
char **gfarm_strarray_dup(char **);
void gfarm_strarray_free(char **);
int gfarm_attach_debugger(void);
