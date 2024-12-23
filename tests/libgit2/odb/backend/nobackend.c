#include "clar_libgit2.h"
#include "repository.h"
#include "odb.h"
#include "git2/sys/repository.h"

static git_repository *_repo;

void test_odb_backend_nobackend__initialize(void)
{
	git_config *config;
	git_odb *odb;
	git_refdb *refdb;

#ifdef GIT_EXPERIMENTAL_SHA256
	git_repository_new_options repo_opts = GIT_REPOSITORY_NEW_OPTIONS_INIT;

	repo_opts.oid_type = GIT_OID_SHA1;

	cl_git_pass(git_repository_new(&_repo, &repo_opts));
#else
	cl_git_pass(git_repository_new(&_repo));
#endif
	cl_git_pass(git_config_new(&config));
	cl_git_pass(git_odb__new(&odb, NULL));
	cl_git_pass(git_refdb_new(&refdb, _repo));

	git_repository_set_config(_repo, config);
	git_repository_set_odb(_repo, odb);
	git_repository_set_refdb(_repo, refdb);

	/* The set increases the refcount and we don't want them anymore */
	git_config_free(config);
	git_odb_free(odb);
	git_refdb_free(refdb);
}

void test_odb_backend_nobackend__cleanup(void)
{
	git_repository_free(_repo);
}

void test_odb_backend_nobackend__write_fails_gracefully(void)
{
	git_oid id;
	git_odb *odb;
	const git_error *err;

	git_repository_odb(&odb, _repo);
	cl_git_fail(git_odb_write(&id, odb, "Hello world!\n", 13, GIT_OBJECT_BLOB));

	err = git_error_last();
	cl_assert_equal_s(err->message, "cannot write object - unsupported in the loaded odb backends");

	git_odb_free(odb);
}
