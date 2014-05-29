/*
 * load-file.cc
 *
 * Utility helper function -- load scheme code from a file
 * Copyright (c) 2008 Linas Vepstas <linasvepstas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_GUILE

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <boost/filesystem/operations.hpp>

#include <opencog/guile/SchemeEval.h>
#include <opencog/util/Config.h>
#include <opencog/util/Logger.h>
#include <opencog/util/misc.h>

namespace opencog {

/**
 * Load scheme code from a file.
 * The code will be loaded into a running instance of the evaluator.
 * Parsing errors will be printed to stderr.
 *
 * Return errno if file cannot be opened.
 */
int load_scm_file (AtomSpace& as, const char * filename)
{
#define BUFSZ 4120
	char buff[BUFSZ];

	FILE * fh = fopen (filename, "r");
	if (NULL == fh)
	{
		int norr = errno;
		fprintf(stderr, "Error: %d %s: %s\n",
			norr, strerror(norr), filename);
		return norr;
	}

	SchemeEval* evaluator = new SchemeEval(&as);
	int lineno = 0;
	int pending_lineno = 0;

	while (1)
	{
		char * rc = fgets(buff, BUFSZ, fh);
		if (NULL == rc) break;
		std::string rv = evaluator->eval(buff);

		if (evaluator->eval_error())
		{
			fprintf(stderr, "File: %s line: %d\n", filename, lineno);
			fprintf(stderr, "%s\n", rv.c_str());
			delete evaluator;
			return 1;
		}
		lineno ++;
		// Keep a record of where pending input starts
		if (!evaluator->input_pending()) pending_lineno = lineno;
	}

	if (evaluator->input_pending())
	{
		// Pending input... print error
		fprintf(stderr, "Warning file %s ended with unterminated "
		        "input begun at line %d\n", filename, pending_lineno);
		delete evaluator;
		return 1;
	}

	fclose(fh);
	delete evaluator;
	return 0;
}

/**
 * Pull the names of scm files out of the config file, the SCM_PRELOAD
 * key, and try to load those, relative to the search paths
 */
void load_scm_files_from_config(AtomSpace& atomSpace, const char* search_paths[])
{
    // Load scheme modules specified in the config file
    std::vector<std::string> scm_modules;
    tokenize(config()["SCM_PRELOAD"], std::back_inserter(scm_modules), ", ");

    std::vector<std::string>::const_iterator it;
    for (it = scm_modules.begin(); it != scm_modules.end(); ++it)
    {
        int rc = 2;
        const char * mod = (*it).c_str();
        if ( search_paths != NULL ) {
            for (int i = 0; search_paths[i] != NULL; ++i) {
                boost::filesystem::path modulePath(search_paths[i]);
                modulePath /= *it;
                if (boost::filesystem::exists(modulePath)) {
                    mod = modulePath.string().c_str();
                    rc = load_scm_file(atomSpace, mod);
                    if (0 == rc) break;
                }
            }
        } // if
        if (rc)
        {
           logger().warn("Failed to load %s: %d %s",
                 mod, rc, strerror(rc));
        }
        else
        {
            logger().info("Loaded %s", mod);
        }
    }
}

}
#endif /* HAVE_GUILE */
