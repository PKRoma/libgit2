#include "clar_libgit2.h"
#include <git2.h>
#include "mwindow.h"
#include "pack.h"
#include "hashmap.h"

extern git_mwindow_packmap git_mwindow__pack_cache;

void test_pack_sharing__open_two_repos(void)
{
	git_repository *repo1, *repo2;
	git_object *obj1, *obj2;
	git_oid id;
	struct git_pack_file *pack;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	cl_git_pass(git_repository_open(&repo1, cl_fixture("testrepo.git")));
	cl_git_pass(git_repository_open(&repo2, cl_fixture("testrepo.git")));

	git_oid_from_string(&id, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750", GIT_OID_SHA1);

	cl_git_pass(git_object_lookup(&obj1, repo1, &id, GIT_OBJECT_ANY));
	cl_git_pass(git_object_lookup(&obj2, repo2, &id, GIT_OBJECT_ANY));

	while (git_mwindow_packmap_iterate(&iter, NULL, &pack, &git_mwindow__pack_cache) == 0)
		cl_assert_equal_i(2, pack->refcount.val);

	cl_assert_equal_i(3, git_mwindow_packmap_size(&git_mwindow__pack_cache));

	git_object_free(obj1);
	git_object_free(obj2);
	git_repository_free(repo1);
	git_repository_free(repo2);

	/* we don't want to keep the packs open after the repos go away */
	cl_assert_equal_i(0, git_mwindow_packmap_size(&git_mwindow__pack_cache));
}
