#include "clar_libgit2.h"
#include "path.h"
#include "posix.h"
#include "status_helpers.h"
#include "util.h"
#include "status.h"

static git_repository *g_repo = NULL;

void test_status_renames__initialize(void)
{
	g_repo = cl_git_sandbox_init("renames");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);
}

void test_status_renames__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void _rename_helper(
	git_repository *repo, const char *from, const char *to, const char *extra)
{
	git_str oldpath = GIT_STR_INIT, newpath = GIT_STR_INIT;

	cl_git_pass(git_str_joinpath(
		&oldpath, git_repository_workdir(repo), from));
	cl_git_pass(git_str_joinpath(
		&newpath, git_repository_workdir(repo), to));

	cl_git_pass(p_rename(oldpath.ptr, newpath.ptr));

	if (extra)
		cl_git_append2file(newpath.ptr, extra);

	git_str_dispose(&oldpath);
	git_str_dispose(&newpath);
}

#define rename_file(R,O,N) _rename_helper((R), (O), (N), NULL)
#define rename_and_edit_file(R,O,N) \
	_rename_helper((R), (O), (N), "Added at the end to keep similarity!")

struct status_entry {
	git_status_t status;
	const char *oldname;
	const char *newname;
};

static void check_status(
	git_status_list *status_list,
	struct status_entry *expected_list,
	size_t expected_len)
{
	const git_status_entry *actual;
	const struct status_entry *expected;
	const char *oldname, *newname;
	size_t i, files_in_status = git_status_list_entrycount(status_list);

	cl_assert_equal_sz(expected_len, files_in_status);

	for (i = 0; i < expected_len; i++) {
		actual = git_status_byindex(status_list, i);
		expected = &expected_list[i];

		oldname = actual->head_to_index ? actual->head_to_index->old_file.path :
			actual->index_to_workdir ? actual->index_to_workdir->old_file.path : NULL;

		newname = actual->index_to_workdir ? actual->index_to_workdir->new_file.path :
			actual->head_to_index ? actual->head_to_index->new_file.path : NULL;

		cl_assert_equal_i_fmt(expected->status, actual->status, "%04x");

		if (expected->oldname) {
			cl_assert(oldname != NULL);
			cl_assert_equal_s(oldname, expected->oldname);
		} else {
			cl_assert(oldname == NULL);
		}

		if (actual->status & (GIT_STATUS_INDEX_RENAMED|GIT_STATUS_WT_RENAMED)) {
			if (expected->newname) {
				cl_assert(newname != NULL);
				cl_assert_equal_s(newname, expected->newname);
			} else {
				cl_assert(newname == NULL);
			}
		}
	}
}

void test_status_renames__head2index_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "newname.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "newname.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "newname.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "aaa.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "untimely.txt", "bbb.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "songof7cities.txt", "ccc.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "ddd.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "ddd.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "aaa.txt");
	rename_file(g_repo, "songof7cities.txt", "ccc.txt");
	rename_and_edit_file(g_repo, "untimely.txt", "bbb.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "ddd.txt"));
	cl_git_pass(git_index_add_bypath(index, "aaa.txt"));
	cl_git_pass(git_index_add_bypath(index, "ccc.txt"));
	cl_git_pass(git_index_add_bypath(index, "bbb.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 4);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_no_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_MODIFIED, "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_MODIFIED, "sixserving.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__head2index_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED, "sixserving.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__index2workdir_one(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "newname.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	rename_file(g_repo, "ikeepsix.txt", "newname.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 1);
	git_status_list_free(statuslist);
}

void test_status_renames__index2workdir_two(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "sixserving.txt", "aaa.txt" },
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "untimely.txt", "bbb.txt" },
		{ GIT_STATUS_WT_RENAMED, "songof7cities.txt", "ccc.txt" },
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "ddd.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	rename_file(g_repo, "ikeepsix.txt", "ddd.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "aaa.txt");
	rename_file(g_repo, "songof7cities.txt", "ccc.txt");
	rename_and_edit_file(g_repo, "untimely.txt", "bbb.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 4);
	git_status_list_free(statuslist);
}

void test_status_renames__index2workdir_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_RENAMED, "sixserving.txt", "ikeepsix.txt" },
		{ GIT_STATUS_WT_RENAMED, "ikeepsix.txt", "sixserving.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "newname-workdir.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "newname-index.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "newname-index.txt"));
	cl_git_pass(git_index_write(index));

	rename_file(g_repo, "newname-index.txt", "newname-workdir.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "ikeepsix-both.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "sixserving-index.txt" },
		{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "songof7cities.txt", "songof7cities-workdir.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimely-both.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_and_edit_file(g_repo, "ikeepsix.txt", "ikeepsix-index.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "sixserving-index.txt");
	rename_file(g_repo, "untimely.txt", "untimely-index.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "ikeepsix-index.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving-index.txt"));
	cl_git_pass(git_index_add_bypath(index, "untimely-index.txt"));
	cl_git_pass(git_index_write(index));

	rename_and_edit_file(g_repo, "ikeepsix-index.txt", "ikeepsix-both.txt");
	rename_and_edit_file(g_repo, "songof7cities.txt", "songof7cities-workdir.txt");
	rename_file(g_repo, "untimely-index.txt", "untimely-both.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 4);
	git_status_list_free(statuslist);

	git_index_free(index);
}


void test_status_renames__both_rename_from_rewrite(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "songof7cities.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "sixserving.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "sixserving.txt", "songof7cities.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	rename_file(g_repo, "ikeepsix.txt", "_temp_.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "songof7cities.txt", "sixserving.txt");
	rename_file(g_repo, "_temp_.txt", "songof7cities.txt");

	cl_git_pass(git_index_add_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_add_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_write(index));

	rename_file(g_repo, "songof7cities.txt", "_temp_.txt");
	rename_file(g_repo, "ikeepsix.txt", "songof7cities.txt");
	rename_file(g_repo, "sixserving.txt", "ikeepsix.txt");
	rename_file(g_repo, "_temp_.txt", "sixserving.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 3);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__rewrites_only_for_renames(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected[] = {
		{ GIT_STATUS_WT_MODIFIED, "ikeepsix.txt", "ikeepsix.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES;

	cl_git_pass(git_repository_index(&index, g_repo));

	cl_git_rewritefile("renames/ikeepsix.txt",
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n" \
		"This is enough content for the file to be rewritten.\n");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 1);
	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_casechange_one(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	int index_caps;
	struct status_entry expected_icase[] = {
		{ GIT_STATUS_INDEX_RENAMED,
		  "ikeepsix.txt", "IKeepSix.txt" },
	};
	struct status_entry expected_case[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "ikeepsix.txt", "IKEEPSIX.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));
	index_caps = git_index_caps(index);

	rename_file(g_repo, "ikeepsix.txt", "IKeepSix.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_add_bypath(index, "IKeepSix.txt"));
	cl_git_pass(git_index_write(index));

	/* on a case-insensitive file system, this change won't matter.
	 * on a case-sensitive one, it will.
	 */
	rename_file(g_repo, "IKeepSix.txt", "IKEEPSIX.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));

	check_status(statuslist, (index_caps & GIT_INDEX_CAPABILITY_IGNORE_CASE) ?
		expected_icase : expected_case, 1);

	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__both_casechange_two(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	int index_caps;
	struct status_entry expected_icase[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "IKeepSix.txt" },
		{ GIT_STATUS_INDEX_MODIFIED,
		  "sixserving.txt", "sixserving.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "songof7cities.txt", "songof7.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimeliest.txt" }
	};
	struct status_entry expected_case[] = {
		{ GIT_STATUS_INDEX_RENAMED |
		  GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_RENAMED,
		  "songof7cities.txt", "SONGOF7.txt" },
		{ GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_RENAMED,
		  "sixserving.txt", "SixServing.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_MODIFIED |
		  GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED,
		  "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED,
		  "untimely.txt", "untimeliest.txt" }
	};

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_repository_index(&index, g_repo));
	index_caps = git_index_caps(index);

	rename_and_edit_file(g_repo, "ikeepsix.txt", "IKeepSix.txt");
	rename_and_edit_file(g_repo, "sixserving.txt", "sixserving.txt");
	rename_file(g_repo, "songof7cities.txt", "songof7.txt");
	rename_file(g_repo, "untimely.txt", "untimelier.txt");

	cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
	cl_git_pass(git_index_remove_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_remove_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_remove_bypath(index, "untimely.txt"));
	cl_git_pass(git_index_add_bypath(index, "IKeepSix.txt"));
	cl_git_pass(git_index_add_bypath(index, "sixserving.txt"));
	cl_git_pass(git_index_add_bypath(index, "songof7.txt"));
	cl_git_pass(git_index_add_bypath(index, "untimelier.txt"));
	cl_git_pass(git_index_write(index));

	rename_and_edit_file(g_repo, "IKeepSix.txt", "ikeepsix.txt");
	rename_file(g_repo, "sixserving.txt", "SixServing.txt");
	rename_and_edit_file(g_repo, "songof7.txt", "SONGOF7.txt");
	rename_file(g_repo, "untimelier.txt", "untimeliest.txt");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));

	check_status(statuslist, (index_caps & GIT_INDEX_CAPABILITY_IGNORE_CASE) ?
		expected_icase : expected_case, 4);

	git_status_list_free(statuslist);

	git_index_free(index);
}

void test_status_renames__zero_byte_file_does_not_fail(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;

	struct status_entry expected[] = {
		{ GIT_STATUS_WT_DELETED, "ikeepsix.txt", "ikeepsix.txt" },
		{ GIT_STATUS_WT_NEW, "zerobyte.txt", "zerobyte.txt" },
	};

	opts.flags |= GIT_STATUS_OPT_RENAMES_FROM_REWRITES |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
		GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR |
		GIT_STATUS_OPT_INCLUDE_IGNORED |
		GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
		GIT_STATUS_SHOW_INDEX_AND_WORKDIR |
		GIT_STATUS_OPT_RECURSE_IGNORED_DIRS;

	p_unlink("renames/ikeepsix.txt");
	cl_git_mkfile("renames/zerobyte.txt", "");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 2);
	git_status_list_free(statuslist);
}

#ifdef GIT_I18N_ICONV
static char *nfc = "\xC3\x85\x73\x74\x72\xC3\xB6\x6D";
static char *nfd = "\x41\xCC\x8A\x73\x74\x72\x6F\xCC\x88\x6D";
#endif

/*
 * Create a file in NFD (canonically decomposed) format.  Ensure
 * that when core.precomposeunicode is false that we return paths
 * in NFD, but when core.precomposeunicode is true, then we
 * return paths precomposed (in NFC).
 */
void test_status_renames__precomposed_unicode_rename(void)
{
#ifdef GIT_I18N_ICONV
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected0[] = {
		{ GIT_STATUS_WT_NEW, nfd, NULL },
		{ GIT_STATUS_WT_DELETED, "sixserving.txt", NULL },
	};
	struct status_entry expected1[] = {
		{ GIT_STATUS_WT_RENAMED, "sixserving.txt", nfd },
	};
	struct status_entry expected2[] = {
		{ GIT_STATUS_WT_DELETED, "sixserving.txt", NULL },
		{ GIT_STATUS_WT_NEW, nfc, NULL },
	};
	struct status_entry expected3[] = {
		{ GIT_STATUS_WT_RENAMED, "sixserving.txt", nfc },
	};

	rename_file(g_repo, "sixserving.txt", nfd);

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;

	cl_repo_set_bool(g_repo, "core.precomposeunicode", false);

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected0, ARRAY_SIZE(expected0));
	git_status_list_free(statuslist);

	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected1, ARRAY_SIZE(expected1));
	git_status_list_free(statuslist);

	cl_repo_set_bool(g_repo, "core.precomposeunicode", true);

	opts.flags &= ~GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected2, ARRAY_SIZE(expected2));
	git_status_list_free(statuslist);

	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected3, ARRAY_SIZE(expected3));
	git_status_list_free(statuslist);
#endif
}

void test_status_renames__precomposed_unicode_toggle_is_rename(void)
{
#ifdef GIT_I18N_ICONV
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	struct status_entry expected0[] = {
		{ GIT_STATUS_INDEX_RENAMED, "ikeepsix.txt", nfd },
	};
	struct status_entry expected1[] = {
		{ GIT_STATUS_WT_RENAMED, nfd, nfc },
	};
	struct status_entry expected2[] = {
		{ GIT_STATUS_INDEX_RENAMED, nfd, nfc },
	};
	struct status_entry expected3[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED, nfd, nfd },
	};

	cl_repo_set_bool(g_repo, "core.precomposeunicode", false);
	rename_file(g_repo, "ikeepsix.txt", nfd);

	{
		git_index *index;
		cl_git_pass(git_repository_index(&index, g_repo));
		cl_git_pass(git_index_remove_bypath(index, "ikeepsix.txt"));
		cl_git_pass(git_index_add_bypath(index, nfd));
		cl_git_pass(git_index_write(index));
		git_index_free(index);
	}

	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
		GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected0, ARRAY_SIZE(expected0));
	git_status_list_free(statuslist);

	cl_repo_commit_from_index(NULL, g_repo, NULL, 0, "commit nfd");

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	cl_assert_equal_sz(0, git_status_list_entrycount(statuslist));
	git_status_list_free(statuslist);

	cl_repo_set_bool(g_repo, "core.precomposeunicode", true);

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected1, ARRAY_SIZE(expected1));
	git_status_list_free(statuslist);

	{
		git_index *index;
		cl_git_pass(git_repository_index(&index, g_repo));
		cl_git_pass(git_index_remove_bypath(index, nfd));
		cl_git_pass(git_index_add_bypath(index, nfc));
		cl_git_pass(git_index_write(index));
		git_index_free(index);
	}

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected2, ARRAY_SIZE(expected2));
	git_status_list_free(statuslist);

	cl_repo_set_bool(g_repo, "core.precomposeunicode", false);

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected3, ARRAY_SIZE(expected3));
	git_status_list_free(statuslist);
#endif
}

void test_status_renames__rename_threshold(void)
{
	git_index *index;
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;

	_rename_helper(g_repo, "ikeepsix.txt", "newname.txt",
		"Line 1\n" \
		"Line 2\n" \
		"Line 3\n" \
		"Line 4\n" \
		"Line 5\n" \
		"Line 6\n" \
		"Line 7\n" \
		"Line 8\n" \
		"Line 9\n"
	);

	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;

	cl_git_pass(git_repository_index(&index, g_repo));

	/* Default threshold */
	{
		struct status_entry expected[] = {
			{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED, "ikeepsix.txt", "newname.txt" },
		};

		cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
		check_status(statuslist, expected, 1);
		git_status_list_free(statuslist);
	}

	/* Threshold set to 90 */
	{
		struct status_entry expected[] = {
			{ GIT_STATUS_WT_DELETED, "ikeepsix.txt", NULL },
			{ GIT_STATUS_WT_NEW, "newname.txt", NULL }
		};

		opts.rename_threshold = 90;

		cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
		check_status(statuslist, expected, 2);
		git_status_list_free(statuslist);
	}

	/* Threshold set to 25 */
	{
		struct status_entry expected[] = {
			{ GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_MODIFIED, "ikeepsix.txt", "newname.txt" },
		};

		opts.rename_threshold = 25;

		cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
		check_status(statuslist, expected, 1);
		git_status_list_free(statuslist);
	}

	git_index_free(index);
}

void test_status_renames__case_insensitive_h2i_and_i2wc(void)
{
	git_status_list *statuslist;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	git_reference *head, *test_branch;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_str path_to_delete = GIT_STR_INIT;
	git_str path_to_edit = GIT_STR_INIT;
	git_index *index;
	git_strarray paths = { NULL, 0 };

	struct status_entry expected[] = {
		{ GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_MODIFIED, "sixserving.txt", "sixserving-renamed.txt" },
		{ GIT_STATUS_INDEX_DELETED, "Wow.txt", "Wow.txt" }
	};


	/* Checkout the correct branch */
	checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_reference_lookup(&head, g_repo, "HEAD"));
	cl_git_pass(git_reference_symbolic_set_target(
		&test_branch, head, "refs/heads/case-insensitive-status", NULL));
	cl_git_pass(git_checkout_head(g_repo, &checkout_opts));

	cl_git_pass(git_repository_index(&index, g_repo));


	/* Rename sixserving.txt, delete Wow.txt, and stage those changes */
	rename_file(g_repo, "sixserving.txt", "sixserving-renamed.txt");
	cl_git_pass(git_str_joinpath(
		&path_to_delete, git_repository_workdir(g_repo), "Wow.txt"));
	cl_git_rmfile(path_to_delete.ptr);

	cl_git_pass(git_index_add_all(index, &paths, GIT_INDEX_ADD_FORCE, NULL, NULL));
	cl_git_pass(git_index_write(index));


	/* Change content of sixserving-renamed.txt */
	cl_git_pass(git_str_joinpath(
		&path_to_edit, git_repository_workdir(g_repo), "sixserving-renamed.txt"));
	cl_git_append2file(path_to_edit.ptr, "New content\n");

	/* Run status */
	opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
	opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	opts.flags |= GIT_STATUS_OPT_SORT_CASE_INSENSITIVELY;

	cl_git_pass(git_status_list_new(&statuslist, g_repo, &opts));
	check_status(statuslist, expected, 2);
	git_status_list_free(statuslist);

	git_index_free(index);

	git_str_dispose(&path_to_delete);
	git_str_dispose(&path_to_edit);

	git_reference_free(head);
	git_reference_free(test_branch);
}
