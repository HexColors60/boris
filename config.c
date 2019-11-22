/**
 * @file config.c
 *
 * Config loader
 *
 * @author Jon Mayo <jon.mayo@gmail.com>
 * @date 2013 Apr 1
 *
 * Copyright 2009-2013 Jon Mayo
 * Ms-RL : See COPYING.txt for complete license text.
 *
 */
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "debug.h"
#include "util.h"
#include "config.h"

/** initialize a config handle. */
void config_setup(struct config *cfg) {
	LIST_INIT(&cfg->watchers);
}

/** free a config handle. */
void config_free(struct config *cfg) {
	struct config_watcher *curr;
	assert(cfg != NULL);
	while((curr=LIST_TOP(cfg->watchers))) {
		LIST_REMOVE(curr, list);
		free(curr->mask);
		free(curr);
	}
}

/**
 * adds a watcher with a shell style mask.
 * func can return 0 to end the chain, or return 1 if the operation should
 * continue on
 */
void config_watch(struct config *cfg, const char *mask, int (*func)(struct config *cfg, void *extra, const char *id, const char *value), void *extra) {
	struct config_watcher *w;
	assert(mask != NULL);
	assert(cfg != NULL);
	w=malloc(sizeof *w);
	w->mask=strdup(mask);
	w->func=func;
	w->extra=extra;
	LIST_INSERT_HEAD(&cfg->watchers, w, list);
}

/** load a configuration using a config handle.
 * a config handle is a set of callbacks and wildcards.
 * */
int config_load(const char *filename, struct config *cfg) {
	char buf[1024];
	FILE *f;
	char *e, *value;
	unsigned line;
	char quote;
	struct config_watcher *curr;

	f=fopen(filename, "r");
	if(!f) {
		PERROR(filename);
		return 0;
	}
	line=0;
	while(line++,fgets(buf, sizeof buf, f)) {
		/* strip comments - honors '' and "" quoting */
		for(e=buf,quote=0;*e;e++) {
			if(!quote && *e=='"')
				quote=*e;
			else if(!quote && *e=='\'')
				quote=*e;
			else if(quote=='\'' && *e=='\'')
				quote=0;
			else if(quote=='"' && *e=='"')
				quote=0;
			else if(!quote && ( *e=='#' || (*e=='/' && e[1]=='/' ))) {
				*e=0; /* found a comment */
				break;
			}
		}

		/* strip trailing white space */
		e=buf+strlen(buf);
		while(e>buf && isspace(*--e)) {
			*e=0;
		}

		/* ignore blank lines */
		if(*buf==0) {
			TRACE("%s:%d:ignoring blank line\n", filename, line);
			continue;
		}

		e=strchr(buf, '=');
		if(!e) {
			/* invalid directive */
			ERROR_FMT("%s:%d:invalid directive\n", filename, line);
			goto failure;
		}

		/* move through the leading space of the value part */
		value=e+1;
		while(isspace(*value)) value++;

		/* strip trailing white space from id part */
		*e=0; /* null terminate the id part */
		while(e>buf && isspace(*--e)) {
			*e=0;
		}

		if(*value=='"') {
			value++;
			e=strchr(value, '"');
			if(e) {
				if(e[1]) {
					ERROR_FMT("%s:%u:error in loading file:trailing garbage after quote\n", filename, line);
					goto failure;
				}
				*e=0;
			} else {
				ERROR_FMT("%s:%u:error in loading file:missing quote\n", filename, line);
				goto failure;
			}
		}

		DEBUG("id='%s' value='%s'\n", buf, value);

		/* check the masks */
		for(curr=LIST_TOP(cfg->watchers);curr;curr=LIST_NEXT(curr, list)) {
			if(!util_fnmatch(curr->mask, buf, UTIL_FNM_CASEFOLD) && curr->func) {
				int res;
				res=curr->func(cfg, curr->extra, buf, value);
				if(!res) {
					break; /* return 0 from the callback will terminate the list */
				} else if(res<0) {
					ERROR_FMT("%s:%u:error in loading file\n", filename, line);
					goto failure;
				}
			}
		}
	}
	fclose(f);
	return 1; /* success */
failure:
	fclose(f);
	return 0; /* failure */
}

#ifndef NTEST
/** test routine to dump a config option. */
static int config_test_show(struct config *cfg UNUSED, void *extra UNUSED, const char *id, const char *value) {
	printf("CONFIG SHOW: %s=%s\n", id, value);
	return 1;
}

/** test the config system. */
static void config_test(void) {
	struct config cfg;
	config_setup(&cfg);
	config_watch(&cfg, "s*er.*", config_test_show, 0);
	config_load("test.cfg", &cfg);
	config_free(&cfg);
}
#endif