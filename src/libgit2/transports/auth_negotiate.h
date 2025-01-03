/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_transports_auth_negotiate_h__
#define INCLUDE_transports_auth_negotiate_h__

#include "common.h"
#include "git2.h"
#include "auth.h"

#ifdef GIT_AUTH_NEGOTIATE

extern int git_http_auth_negotiate(
	git_http_auth_context **out,
	const git_net_url *url);

#else

#define git_http_auth_negotiate git_http_auth_dummy

#endif /* GIT_AUTH_NEGOTIATE */

#endif
