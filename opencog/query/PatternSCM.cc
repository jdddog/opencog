/*
 * PatternSCM.cc
 *
 * Guile Scheme bindings for the pattern matcher.
 * Copyright (c) 2008, 2014 Linas Vepstas <linas@linas.org>
 */

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/guile/SchemePrimitive.h>
#include <opencog/guile/SchemeSmob.h>

#include "PatternMatch.h"
#include "PatternSCM.h"

using namespace opencog;

PatternSCM* PatternSCM::_inst = NULL;

PatternSCM::PatternSCM()
{
	if (NULL == _inst) {
		_inst = this;
		init();
	}
}

PatternSCM::~PatternSCM()
{
}

void PatternSCM::init(void)
{
	_inst = new PatternSCM();
#ifdef HAVE_GUILE
	// XXX FIXME .. what we really should do here is to make sure
	// that SchemeSmob is initialized first ... and, for that, we
	// would need to get our hands on an atomspace ... Ugh. Yuck.
	define_scheme_primitive("cog-bind", &PatternSCM::do_bindlink, _inst);
	define_scheme_primitive("cog-bind-crisp", &PatternSCM::do_crisp_bindlink, _inst);
#endif
}

/**
 * Run implication, assuming that the argument is a handle to
 * an BindLink containing variables and an ImplicationLink
 */
Handle PatternSCM::do_bindlink(Handle h)
{
#ifdef HAVE_GUILE
	// XXX we should also allow opt-args to be a list of handles
	AtomSpace *as = SchemeSmob::ss_get_env_as("cog-bind");
	PatternMatch pm;
	pm.set_atomspace(as);
	Handle grounded_expressions = pm.bindlink(h);
	return grounded_expressions;
#else
	return Handle::UNDEFINED;
#endif
}

/**
 * Run implication, assuming that the argument is a handle to
 * an BindLink containing variables and an ImplicationLink
 */
Handle PatternSCM::do_crisp_bindlink(Handle h)
{
#ifdef HAVE_GUILE
	// XXX we should also allow opt-args to be a list of handles
	AtomSpace *as = SchemeSmob::ss_get_env_as("cog-bind-crisp");
	PatternMatch pm;
	pm.set_atomspace(as);
	Handle grounded_expressions = pm.crisp_logic_bindlink(h);
	return grounded_expressions;
#else
	return Handle::UNDEFINED;
#endif
}
