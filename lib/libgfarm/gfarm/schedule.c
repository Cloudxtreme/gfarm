#include <stdio.h> /* sprintf */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gfarm/gfarm.h>
#include "hash.h"
#include "auth.h" /* XXX gfarm_random_initialize() */
#include "gfs_client.h"

char GFARM_ERR_NO_REPLICA[] = "no replica";
char GFARM_ERR_NO_HOST[] = "no filesystem node";

#define ARCH_SET_HASHTAB_SIZE 31		/* prime number */

#define IS_IN_ARCH_SET(arch, arch_set) \
	(gfarm_hash_lookup(arch_set, arch, strlen(arch) + 1) != NULL)

#define free_arch_set(arch_set)	gfarm_hash_table_free(arch_set)

/* Create a set of architectures that the program is registered for */
static char *
program_arch_set(char *program, struct gfarm_hash_table **arch_setp)
{
	char *e, *gfarm_file;
	struct gfarm_path_info pi;
	struct gfarm_file_section_info *sections;
	struct gfarm_hash_table *arch_set;
	int i, nsections, created;

	e = gfarm_url_make_path(program, &gfarm_file);
	if (e != NULL)
		return (e);
	e = gfarm_path_info_get(gfarm_file, &pi);
	if (e != NULL) {
		if (e == GFARM_ERR_NO_SUCH_OBJECT)
			e = "such program isn't registered";
		free(gfarm_file);
		return (e);
	}
	if (!GFARM_S_IS_PROGRAM(pi.status.st_mode)) {
		gfarm_path_info_free(&pi);
		free(gfarm_file);
		return ("specified command is not an executable");
	}
	e = gfarm_file_section_info_get_all_by_file(gfarm_file,
	    &nsections, &sections);
	gfarm_path_info_free(&pi);
	free(gfarm_file);
	if (e != NULL)
		return ("no binary is registered as the specified command");

	arch_set = gfarm_hash_table_alloc(ARCH_SET_HASHTAB_SIZE,
	    gfarm_hash_default, gfarm_hash_key_equal_default);
	if (arch_set == NULL) {
		gfarm_file_section_info_free_all(nsections, sections);
		return (GFARM_ERR_NO_MEMORY);
	}
	/* register architectures of the program to `arch_set' */
	for (i = 0; i < nsections; i++) {
		if (gfarm_hash_enter(arch_set,
		    sections[i].section, strlen(sections[i].section) + 1,
		    sizeof(int), &created) == NULL) {
			free_arch_set(arch_set);
			gfarm_file_section_info_free_all(nsections, sections);
			return (GFARM_ERR_NO_MEMORY);
		}
	}
	gfarm_file_section_info_free_all(nsections, sections);
	*arch_setp = arch_set;
	return (NULL);
}

#define HOSTS_HASHTAB_SIZE	3079	/* prime number */

#define IS_IN_HOST_SET(hostname, host_set) \
	(gfarm_hash_lookup(host_set, hostname, strlen(hostname) + 1) != NULL)

#define free_host_set(host_set)	gfarm_hash_table_free(host_set)

/* Create a set of hosts which can run the program */
static char *
host_set_by_arch_set(int nhosts, struct gfarm_host_info *hosts,
	struct gfarm_hash_table *arch_set,
	int *host_set_sizep, struct gfarm_hash_table **host_setp)
{
	struct gfarm_hash_table *host_set;
	int i, created, host_set_size;

	host_set = gfarm_hash_table_alloc(HOSTS_HASHTAB_SIZE,
	    gfarm_hash_default, gfarm_hash_key_equal_default);
	if (host_set == NULL)
		return (GFARM_ERR_NO_MEMORY);

	host_set_size = 0;
	for (i = 0; i < nhosts; i++) {
		if (IS_IN_ARCH_SET(hosts[i].architecture, arch_set)) {
			if (gfarm_hash_enter(host_set,
			    hosts[i].hostname, strlen(hosts[i].hostname) + 1,
			    sizeof(int), &created) == NULL) {
				free_host_set(host_set);
				return (GFARM_ERR_NO_MEMORY);
			}
			host_set_size++;
		}
	}
	if (host_set_size == 0) {
		free_host_set(host_set);
		return ("there is no host which can run the program");
	}
	*host_set_sizep = host_set_size;
	*host_setp = host_set;
	return (NULL);
}

static char *
hosts_for_program(char *program,
	int *n_all_hostsp, struct gfarm_host_info **all_hostsp,
	int *host_set_sizep, struct gfarm_hash_table **host_setp)
{
	char *e;
	int n_all_hosts, n_runnable_hosts;
	struct gfarm_host_info *all_hosts;
	struct gfarm_hash_table *arch_set, *runnable_host_set;

	e = program_arch_set(program, &arch_set);
	if (e == NULL) {
		e = gfarm_host_info_get_all(&n_all_hosts, &all_hosts);
		if (e == NULL) {
			e = host_set_by_arch_set(
			    n_all_hosts, all_hosts, arch_set,
			    &n_runnable_hosts, &runnable_host_set);
			if (e == NULL) {
				*n_all_hostsp = n_all_hosts;
				*all_hostsp = all_hosts;
				*host_set_sizep = n_runnable_hosts;
				*host_setp = runnable_host_set;
				free_arch_set(arch_set);
				return (NULL);
			}
			gfarm_host_info_free_all(n_all_hosts, all_hosts);
		}
		free_arch_set(arch_set);
	}
	return (e);
}

/*
 * search_idle() callback routines to get next hostname
 */

/* abstract super class (a.k.a. interface class) to get next hostname */

struct get_next_iterator {
	char *(*get_next)(struct get_next_iterator *);
};

/* for "char *" */

struct string_array_iterator {
	struct get_next_iterator super;
	char **current;
};

static char *
get_next_from_string_array(struct get_next_iterator *self)
{
	struct string_array_iterator *iterator =
	    (struct string_array_iterator *)self;

	return (*iterator->current++);
}

static struct get_next_iterator *
init_string_array_iterator(struct string_array_iterator *iterator,
	char **strings)
{
	iterator->super.get_next = get_next_from_string_array;
	iterator->current = strings;
	return (&iterator->super);
}

/* for struct gfarm_host_info */

struct host_info_array_iterator {
	struct get_next_iterator super;
	struct gfarm_host_info *current;
};

static char *
get_next_from_host_info_array(struct get_next_iterator *self)
{
	struct host_info_array_iterator *iterator =
	    (struct host_info_array_iterator *)self;

	return ((iterator->current++)->hostname);
}

static struct get_next_iterator *
init_host_info_array_iterator(struct host_info_array_iterator *iterator,
	struct gfarm_host_info *hosts)
{
	iterator->super.get_next = get_next_from_host_info_array;
	iterator->current = hosts;
	return (&iterator->super);
}

/* for struct gfarm_file_section_copy_info */

struct section_copy_info_array_iterator {
	struct get_next_iterator super;
	struct gfarm_file_section_copy_info *current;
};

static char *
get_next_from_section_copy_info_array(struct get_next_iterator *self)
{
	struct section_copy_info_array_iterator *iterator =
	    (struct section_copy_info_array_iterator *)self;

	return ((iterator->current++)->hostname);
}

static struct get_next_iterator *
init_section_copy_info_array_iterator(
	struct section_copy_info_array_iterator *iterator,
	struct gfarm_file_section_copy_info *copies)
{
	iterator->super.get_next = get_next_from_section_copy_info_array;
	iterator->current = copies;
	return (&iterator->super);
}

/* abstract super class (a.k.a. interface class) to restrict host */

struct string_filter {
	int (*suitable)(struct string_filter *self, const char *);
};

/* to not restrict anything */

static int
always_suitable(struct string_filter *self, const char *string)
{
	return (1);
}

static struct string_filter null_filter = { always_suitable };

/* to restrict host by domain */

struct domainname_filter {
	struct string_filter super;
	const char *domainname;
};

static int
is_host_in_domain(struct string_filter *self, const char *hostname)
{
	struct domainname_filter *filter = (struct domainname_filter *)self;

	return (gfarm_host_is_in_domain(hostname, filter->domainname));
}

static struct string_filter *
init_domainname_filter(struct domainname_filter *filter,
	const char *domainname)
{
	filter->super.suitable = is_host_in_domain;
	filter->domainname = domainname;
	return (&filter->super);
}

/* to restrict string by string_set */

struct string_set_filter {
	struct string_filter super;
	struct gfarm_hash_table *string_set;
};

static int
is_string_in_set(struct string_filter *self, const char *string)
{
	struct string_set_filter *filter = (struct string_set_filter *)self;

	return (gfarm_hash_lookup(filter->string_set,
	    string, strlen(string) + 1) != NULL);
}

static struct string_filter *
init_string_set_filter(struct string_set_filter *filter,
	struct gfarm_hash_table *string_set)
{
	filter->super.suitable = is_string_in_set;
	filter->string_set = string_set;
	return (&filter->super);
}

/*
 * gfarm_client_*_load_*() callback routine which is provided by search_idle()
 */

#define IDLE_LOAD_AVERAGE	0.1F
#define SEMI_IDLE_LOAD_AVERAGE	0.5F

struct search_idle_available_host {
	char *hostname;
	float loadavg;
};

struct search_idle_state {
	struct search_idle_available_host *available_hosts;
	int available_hosts_number;
	int idle_hosts_number;
	int semi_idle_hosts_number;
};

struct search_idle_callback_closure {
	struct search_idle_state *state;
	char *hostname;
};

static void
search_idle_callback(void *closure, struct sockaddr *addr,
	struct gfs_client_load *load, char *error)
{
	struct search_idle_callback_closure *c = closure;
	struct search_idle_state *s = c->state;

	if (error == NULL) {
		struct search_idle_available_host *h =
		    &s->available_hosts[s->available_hosts_number++];

		h->hostname = c->hostname;
		h->loadavg = load->loadavg_1min;
		if (h->loadavg <= SEMI_IDLE_LOAD_AVERAGE) {
			s->semi_idle_hosts_number++;
			if (h->loadavg <= IDLE_LOAD_AVERAGE)
				s->idle_hosts_number++;
		}
	}
	free(c);
}

static int
loadavg_compare(const void *a, const void *b)
{
	const struct search_idle_available_host *p = a;
	const struct search_idle_available_host *q = b;
	const float l1 = p->loadavg;
	const float l2 = q->loadavg;

	if (l1 < l2)
		return (-1);
        else if (l1 > l2)
		return (1);
	else
		return (0);
}

/*
 * The search will be stopped, if `enough_number' of semi-idle hosts
 * are found.
 *
 * `*nohostsp' is an IN/OUT parameter.
 * It means desired number of hosts as an INPUT parameter.
 * It returns available number of hosts as an OUTPUT parameter.
 */
static char *
search_idle(int concurrency, int enough_number,
	int nihosts, struct string_filter *ihost_filter,
	struct get_next_iterator *ihost_iterator,
	int *nohostsp, char **ohosts)
{
	char *e, *ihost;
	int i, desired_number = *nohostsp;
	struct gfs_client_udp_requests *udp_requests;
	struct sockaddr addr;
	struct search_idle_state s;
	struct search_idle_callback_closure *c;

	if (nihosts == 0)
		return (GFARM_ERR_NO_HOST);
	s.available_hosts_number =
	    s.idle_hosts_number = s.semi_idle_hosts_number = 0;
	s.available_hosts = malloc(nihosts * sizeof(*s.available_hosts));
	if (s.available_hosts == NULL)
		return (GFARM_ERR_NO_MEMORY);
	e = gfarm_client_init_load_requests(concurrency, &udp_requests);
	if (e != NULL) {
		free(s.available_hosts);
		return (e);
	}
	for (i = 0; i < nihosts; i++) {
		do {
			ihost = (*ihost_iterator->get_next)(ihost_iterator);
		} while (!(*ihost_filter->suitable)(ihost_filter, ihost));
		e = gfarm_host_address_get(ihost, gfarm_spool_server_port,
		    &addr, NULL);
		if (e != NULL)
			continue;
		c = malloc(sizeof(*c));
		if (c == NULL)
			break;
		c->state = &s;
		c->hostname = ihost;
		e = gfarm_client_add_load_request(udp_requests, &addr,
		    c, search_idle_callback);
		if (s.idle_hosts_number >= desired_number ||
		    s.semi_idle_hosts_number >= enough_number)
			break;
	}
	e = gfarm_client_wait_all_load_results(udp_requests);
	if (s.available_hosts_number == 0) {
		free(s.available_hosts);
		*nohostsp = 0;
		return (GFARM_ERR_NO_HOST);
	}

	/* sort hosts in the order of load average */
	qsort(s.available_hosts, s.available_hosts_number,
	    sizeof(*s.available_hosts), loadavg_compare);

	for (i = 0; i < s.available_hosts_number && i < desired_number; i++)
		ohosts[i] = s.available_hosts[i].hostname;
	*nohostsp = i;
	free(s.available_hosts);

	return (NULL);
}

/*
 * shuffle input-hosts before searching to avoid unbalanced search
 */

/* #define USE_SHUFFLED */

#ifdef USE_SHUFFLED

static void
shuffle_strings(int n, char **strings)
{
	int i, j;
	char *tmp;

	gfarm_random_initialize();
	for (i = n - 1; i > 0; --i) {
#ifdef HAVE_RANDOM
		j = random() % (i + 1);
#else
		j = rand()/(RAND_MAX + 1.0) * (i + 1);
#endif
		tmp = strings[i];
		strings[i] = strings[j];
		strings[j] = tmp;
	}
}

static char *
search_idle_shuffled(int concurrency, int enough_number,
	int nihosts, struct string_filter *ihost_filter,
	struct get_next_iterator *ihost_iterator,
	int *nohostsp, char **ohosts)
{
	char *e, *ihost, **shuffled_ihosts;
	int i;
	struct string_array_iterator host_iterator;

	if (nihosts == 0)
		return (GFARM_ERR_NO_HOST);
	shuffled_ihosts = malloc(nihosts * sizeof(*shuffled_ihosts));
	if (shuffled_ihosts == NULL)
		return (GFARM_ERR_NO_MEMORY);
	for (i = 0; i < nihosts; i++) {
		do {
			ihost = (*ihost_iterator->get_next)(ihost_iterator);
		} while (!(*ihost_filter->suitable)(ihost_filter, ihost));
		shuffled_ihosts[i++] = ihost;
	}
	shuffle_strings(nihosts, shuffled_ihosts);

	e = search_idle(concurrency, enough_number, nihosts, &null_filter,
	    init_string_array_iterator(&host_iterator, shuffled_ihosts),
	    nohostsp, ohosts);
	free(shuffled_ihosts);
	return (e);
}

#endif /* USE_SHUFFLED */

void
gfarm_strings_expand_cyclic(int nsrchosts, char **srchosts,
	int ndsthosts, char **dsthosts)
{
	int i, j;

	for (i = 0, j = 0; i < ndsthosts; i++, j++) {
		if (j >= nsrchosts)
			j = 0;
		dsthosts[i] = srchosts[j];
	}
}

#define CONCURRENCY	10
#define ENOUGH_RATE	4

static char *
search_idle_cyclic(
	int nihosts, struct string_filter *ihost_filter,
	struct get_next_iterator *ihost_iterator,
	int nohosts, char **ohosts)
{
	char *e;
	int nfound = nohosts;

#ifdef USE_SHUFFLED
	e = search_idle_shuffled(CONCURRENCY, nohosts * ENOUGH_RATE,
	    nihosts, ihost_filter, ihost_iterator,
	    &nfound, ohosts);
#else
	e = search_idle(CONCURRENCY, nohosts * ENOUGH_RATE,
	    nihosts, ihost_filter, ihost_iterator,
	    &nfound, ohosts);
#endif

	if (e != NULL)
		return (e);
	if (nfound == 0) {
		/* Oh, my god */
		return (GFARM_ERR_NO_HOST);
	}
	if (nohosts > nfound)
		gfarm_strings_expand_cyclic(nfound, ohosts,
		    nohosts - nfound, &ohosts[nfound]);
	return (NULL);
}

/* 
 * Select 'nohosts' hosts among 'nihosts' ihosts in the order of
 * load average, and return to 'ohosts'.
 * When enough number of hosts are not available, the available hosts
 * will be listed in the cyclic manner.
 */
char *
gfarm_schedule_search_idle_hosts(
	int nihosts, char **ihosts, int nohosts, char **ohosts)
{
	struct string_array_iterator host_iterator;

	return (search_idle_cyclic(nihosts, &null_filter,
	    init_string_array_iterator(&host_iterator, ihosts),
	    nohosts, ohosts));
}

char *
gfarm_schedule_search_idle_by_all(int nohosts, char **ohosts)
{
	char *e;
	int n_all_hosts;
	struct gfarm_host_info *all_hosts;
	struct host_info_array_iterator host_iterator;

   	e = gfarm_host_info_get_all(&n_all_hosts, &all_hosts);
	if (e != NULL)
		return (e);
	e = search_idle_cyclic(n_all_hosts, &null_filter,
	    init_host_info_array_iterator(&host_iterator, all_hosts),
	    nohosts, ohosts);
	if (e == NULL)
		e = gfarm_fixedstrings_dup(nohosts, ohosts, ohosts);
	gfarm_host_info_free_all(n_all_hosts, all_hosts);
	return (e);
}

/*
 * lists host names that contains domainname.
 */
char *
gfarm_schedule_search_idle_by_domainname(const char *domainname,
	int nohosts, char **ohosts)
{
	char *e;
	int i, nhosts, n_all_hosts;
	struct gfarm_host_info *all_hosts;
	struct host_info_array_iterator host_iterator;
	struct domainname_filter domain_filter;

	e = gfarm_host_info_get_all(&n_all_hosts, &all_hosts);
	if (e != NULL)
		return (e);

	nhosts = 0;
	for (i = 0; i < n_all_hosts; i++) {
		if (gfarm_host_is_in_domain(all_hosts[i].hostname, domainname))
			++nhosts;
	}

	e = search_idle_cyclic(nhosts,
	    init_domainname_filter(&domain_filter, domainname),
	    init_host_info_array_iterator(&host_iterator, all_hosts),
	    nohosts, ohosts);
	if (e == NULL)
		e = gfarm_fixedstrings_dup(nohosts, ohosts, ohosts);
	gfarm_host_info_free_all(n_all_hosts, all_hosts);
	return (e);
}

char *
gfarm_schedule_search_idle_by_program(char *program,
	int nohosts, char **ohosts)
{
	char *e;
	int n_all_hosts, n_runnable_hosts;
	struct gfarm_host_info *all_hosts;
	struct gfarm_hash_table *runnable_host_set;
	struct host_info_array_iterator host_iterator;
	struct string_set_filter host_filter;

	if (!gfarm_is_url(program))
		return (gfarm_schedule_search_idle_by_all(nohosts, ohosts));

	e = hosts_for_program(program, &n_all_hosts, &all_hosts,
	    &n_runnable_hosts, &runnable_host_set);
	if (e != NULL)
		return (e);
	e = search_idle_cyclic(n_runnable_hosts,
	    init_string_set_filter(&host_filter, runnable_host_set),
	    init_host_info_array_iterator(&host_iterator, all_hosts),
	    nohosts, ohosts);
	if (e == NULL)
		e = gfarm_fixedstrings_dup(nohosts, ohosts, ohosts);
	free_host_set(runnable_host_set);
	gfarm_host_info_free_all(n_all_hosts, all_hosts);
	return (e);
}

/*
 * Return GFARM_ERR_NO_HOST, if there is a replica, but there isn't
 * any host which satisfies the hostname_filter.
 */
static char *
search_idle_by_section_copy_info(
	int ncopies, struct gfarm_file_section_copy_info *copies,
	struct string_filter *hostname_filter,
	int nohosts, char **ohosts)
{
	int i, nhosts;
	struct section_copy_info_array_iterator copy_iterator;

	nhosts = 0;
	for (i = 0; i < ncopies; i++) {
		if ((*hostname_filter->suitable)(hostname_filter,
		    copies[i].hostname))
			nhosts++;
	}
	if (nhosts == 0)
		return (GFARM_ERR_NO_HOST);

	return (search_idle_cyclic(nhosts, hostname_filter,
	    init_section_copy_info_array_iterator(&copy_iterator, copies),
	    nohosts, ohosts));
}

/*
 * Return GFARM_ERR_NO_HOST, if there is a replica, but there isn't
 * any host which satisfies the hostname_filter.
 */
static char *
schedule_by_file_section(char *gfarm_file, char *section,
	struct string_filter *hostname_filter,
	int nohosts, char **ohosts)
{
	char *e;
	int ncopies;
	struct gfarm_file_section_copy_info *copies;

	e = gfarm_file_section_copy_info_get_all_by_section(
	    gfarm_file, section, &ncopies, &copies);
	if (e != NULL)
		return (e);
	e = ncopies == 0 ? GFARM_ERR_NO_REPLICA :
	    search_idle_by_section_copy_info(ncopies, copies, hostname_filter,
	    nohosts, ohosts);
	if (e == NULL)
		e = gfarm_fixedstrings_dup(nohosts, ohosts, ohosts);
	gfarm_file_section_copy_info_free_all(ncopies, copies);
	return (e);
}

char *
gfarm_file_section_host_schedule(char *gfarm_file, char *section, char **hostp)
{
	return (schedule_by_file_section(gfarm_file, section, &null_filter,
	    1, hostp));
}

char *
gfarm_file_section_host_schedule_by_program(char *gfarm_file, char *section,
	char *program, char **hostp)
{
	char *e;
	struct gfarm_hash_table *runnable_host_set;
	struct gfarm_host_info *all_hosts;
	int n_all_hosts, n_runnable_hosts;
	struct string_set_filter host_filter;
	struct host_info_array_iterator host_iterator;

	if (!gfarm_is_url(program))
		return (gfarm_file_section_host_schedule(gfarm_file, section,
		    hostp));

	e = hosts_for_program(program, &n_all_hosts, &all_hosts,
	    &n_runnable_hosts, &runnable_host_set);
	if (e != NULL)
		return (e);
	e = schedule_by_file_section(gfarm_file, section,
	    init_string_set_filter(&host_filter, runnable_host_set),
	    1, hostp);
	if (e == GFARM_ERR_NO_HOST) {
		e = search_idle_cyclic(n_runnable_hosts,
		    init_string_set_filter(&host_filter, runnable_host_set),
		    init_host_info_array_iterator(&host_iterator, all_hosts),
		    1, hostp);
	}
	free_host_set(runnable_host_set);
	gfarm_host_info_free_all(n_all_hosts, all_hosts);
	return (e);
}

char *
gfarm_file_section_host_schedule_with_priority_to_local(
	char *gfarm_file, char *section, char **hostp)
{
	char *e, *host, *self_name;
	int i, ncopies;
	struct gfarm_file_section_copy_info *copies;

	e = gfarm_file_section_copy_info_get_all_by_section(
	    gfarm_file, section, &ncopies, &copies);
	if (e != NULL)
		return (e);
	if (ncopies == 0) {
		gfarm_file_section_copy_info_free_all(ncopies, copies);
		return (GFARM_ERR_NO_REPLICA);
	}

	/* choose/schedule local one, if possible */
	e = gfarm_host_get_canonical_self_name(&self_name);
	if (e != NULL) {
		i = ncopies;
	} else {
		for (i = 0; i < ncopies; i++)
			if (strcasecmp(self_name, copies[i].hostname) == 0)
				break;
	}
	if (i < ncopies) /* local */
		host = copies[i].hostname;
	else {
		e = search_idle_by_section_copy_info(ncopies, copies,
		    &null_filter,
		    1, &host);
		if (e != NULL) {
			gfarm_file_section_copy_info_free_all(ncopies, copies);
			return (e);
		}
	}

	host = strdup(host);
	gfarm_file_section_copy_info_free_all(ncopies, copies);
	if (host == NULL)
		return (GFARM_ERR_NO_MEMORY);
	*hostp = host;
	return (NULL);
}

static char *
gfarm_url_hosts_schedule_filtered(char *gfarm_url, char *option,
	int nihosts, struct string_filter *ihost_filter,
	struct get_next_iterator *ihost_iterator,
	int *nhostsp, char ***hostsp)
{
	char *e, *gfarm_file, **hosts, **residual;
	int i, nfrags, shortage;
	char section[GFARM_INT32STRLEN + 1];

	e = gfarm_url_fragment_number(gfarm_url, &nfrags);
	if (e != NULL)
		return (e);
	e = gfarm_url_make_path(gfarm_url, &gfarm_file);
	if (e != NULL)
		return (e);
	hosts = malloc(sizeof(char *) * nfrags);
	if (hosts == NULL) {
		free(gfarm_file);
		return (GFARM_ERR_NO_MEMORY);
	}
	shortage = 0;
	for (i = 0; i < nfrags; i++) {
		sprintf(section, "%d", i);
		e = schedule_by_file_section(gfarm_file, section, ihost_filter,
		    1, &hosts[i]);
		if (e == GFARM_ERR_NO_HOST) {
			hosts[i] = NULL;
			shortage++;
			continue;
		}
		if (e != NULL) {
			gfarm_strings_free_deeply(i, hosts);
			free(gfarm_file);
			return (e);
		}
	}
	if (shortage > 0) {
		residual = malloc(shortage * sizeof(*residual));
		if (residual == NULL) {
			gfarm_strings_free_deeply(nfrags, hosts);
			free(gfarm_file);
			return (GFARM_ERR_NO_MEMORY);
		}
		e = search_idle_cyclic(nihosts, ihost_filter, ihost_iterator,
		    shortage, residual);
		if (e == NULL)
			e = gfarm_fixedstrings_dup(shortage,residual,residual);
		if (e != NULL) {
			free(residual);
			gfarm_strings_free_deeply(nfrags, hosts);
			free(gfarm_file);
			return (e);
		}
		for (i = 0; i < nfrags; i++) {
			if (hosts[i] == NULL) {
				hosts[i] = residual[--shortage];
				if (shortage == 0)
					break;
			}
		}
		free(residual);
	}
	*nhostsp = nfrags;
	*hostsp = hosts;
	free(gfarm_file);
	return (e);
}

char *
gfarm_url_hosts_schedule(char *gfarm_url, char *option,
	int *nhostsp, char ***hostsp)
{
	char *e, *gfarm_file, **hosts;
	int i, nfrags;
	char section[GFARM_INT32STRLEN + 1];

	e = gfarm_url_fragment_number(gfarm_url, &nfrags);
	if (e != NULL)
		return (e);
	e = gfarm_url_make_path(gfarm_url, &gfarm_file);
	if (e != NULL)
		return (e);
	hosts = malloc(sizeof(char *) * nfrags);
	if (hosts == NULL) {
		free(gfarm_file);
		return (GFARM_ERR_NO_MEMORY);
	}
	for (i = 0; i < nfrags; i++) {
		sprintf(section, "%d", i);
		e = gfarm_file_section_host_schedule(
		    gfarm_file, section, &hosts[i]);
		if (e != NULL) {
			gfarm_strings_free_deeply(i, hosts);
			free(gfarm_file);
			return (e);
		}
	}
	free(gfarm_file);
	*nhostsp = nfrags;
	*hostsp = hosts;
	return (NULL);
}

char *
gfarm_url_hosts_schedule_by_program(
	char *gfarm_url, char *program, char *option,
	int *nhostsp, char ***hostsp)
{
	char *e;
	int n_all_hosts, n_runnable_hosts;
	struct gfarm_host_info *all_hosts;
	struct gfarm_hash_table *runnable_host_set;
	struct string_set_filter host_filter;
	struct host_info_array_iterator host_iterator;

	if (!gfarm_is_url(program))
		return (gfarm_url_hosts_schedule(gfarm_url, option,
		    nhostsp, hostsp));

	e = hosts_for_program(program, &n_all_hosts, &all_hosts,
	    &n_runnable_hosts, &runnable_host_set);
	if (e != NULL)
		return (e);
	e = gfarm_url_hosts_schedule_filtered(gfarm_url, option,
	    n_runnable_hosts,
	    init_string_set_filter(&host_filter, runnable_host_set),
	    init_host_info_array_iterator(&host_iterator, all_hosts),
	    nhostsp, hostsp);
	free_host_set(runnable_host_set);
	gfarm_host_info_free_all(n_all_hosts, all_hosts);
	return (e);
}
