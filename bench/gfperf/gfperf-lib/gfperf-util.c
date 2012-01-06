/*
 * $Id$
 */


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gfperf-lib.h"

long long
strtonum(const char *str)
{
	long long tmp = 0;
	const char *p = str;
	char c;

	while (*p != '\0') {
		c = *p++;
		if (c >= '0' && c <= '9') {
			tmp *= 10;
			tmp += c - '0';
		} else {
			switch (c) {
			case 'k':
			case 'K':
				tmp *= 1<<10;
			break;
			case 'm':
			case 'M':
				tmp *= 1<<20;
			break;
			case 'g':
			case 'G':
				tmp *= 1<<30;
			break;
			case 't':
			case 'T':
				tmp *= (long long)1<<40;
			break;
			case 'p':
			case 'P':
				tmp *= (long long)1<<50;
			break;
			case 'e':
			case 'E':
				tmp *= (long long)1<<60;
			break;
			}
		}
	}
	return (tmp);
}

void
sub_timeval(const struct timeval *a, const struct timeval *b,
	    struct timeval *c)
{

	if (a->tv_usec < b->tv_usec) {
		c->tv_usec = 1000000+a->tv_usec-b->tv_usec;
		c->tv_sec = a->tv_sec-1-b->tv_sec;
	} else {
		c->tv_usec = a->tv_usec-b->tv_usec;
		c->tv_sec = a->tv_sec-b->tv_sec;
	}
}

gfarm_error_t
is_dir_posix(char *path)
{
	struct stat sb;
	int e;

	e = stat(path, &sb);
	if (e != 0)
		return (GFARM_ERR_NOT_A_DIRECTORY);

	if (S_ISDIR(sb.st_mode))
		return (GFARM_ERR_NO_ERROR);
	else
		return (GFARM_ERR_NOT_A_DIRECTORY);
}

gfarm_error_t
is_dir_gfarm(char *path)
{
	struct gfs_stat sb;
	int e;

	if (strncmp(path, "gfarm:///", 9) == 0)
		e = gfs_stat(&path[8], &sb);
	else
		e = gfs_stat(path, &sb);

	if (e != GFARM_ERR_NO_ERROR)
		return (GFARM_ERR_NOT_A_DIRECTORY);

	if (GFARM_S_ISDIR(sb.st_mode)) {
		gfs_stat_free(&sb);
		return (GFARM_ERR_NO_ERROR);
	} else {
		gfs_stat_free(&sb);
		return (GFARM_ERR_NOT_A_DIRECTORY);
	}
}

const char *
find_root_from_url(const char *url)
{
	int i = 0;
	const char *p = url;
	if (*p == '/')
		return (url);
	while (*p != '\0') {
		if (*p == '/') {
			i++;
			if (i >= 3)
				return (p);
		}
		p++;
	}
	return (NULL);
}

int is_file_url(const char *url)
{
	return (strncmp(url, FILE_URL_PREFIX, FILE_URL_PREFIX_LEN) == 0);
}

int parse_utc_time_string(const char *s, time_t *ret)
{
	char *r;
	time_t t;
	struct tm tm;
	static char *fmt = "%Y-%m-%dT%H:%M:%SZ";

	r = strptime(s, fmt, &tm);
	if (r == NULL)
		return (-1);

	t = timegm(&tm);

	*ret = t;

	return (0);
}

int is_file_exist_posix(const char *filename)
{
	struct stat buf;
	int r;

	r = stat(filename, &buf);
	if (r < 0)
		return (0);

	return (1);
}

int is_file_exist_gfarm(const char *filename)
{
	struct gfs_stat buf;
	int r;

	r = gfs_stat(filename, &buf);
	if (r != GFARM_ERR_NO_ERROR)
		return (0);

	gfs_stat_free(&buf);
	return (1);
}
