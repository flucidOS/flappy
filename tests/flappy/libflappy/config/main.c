/*-
 * Copyright (c) 2014 Enno Boland.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-
 */
#include <atf-c.h>
#include <flappy.h>
#include <stdlib.h>
#include <limits.h>

ATF_TC(config_include_test);
ATF_TC_HEAD(config_include_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by file globbing");
}

ATF_TC_BODY(config_include_test, tc)
{
	struct flappy_handle xh;
	const char *tcsdir;
	char *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);

	buf = flappy_xasprintf("%s/flappy.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/flappy.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/1.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/1.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/2.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/2.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);
	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 2);
}


ATF_TC(config_include_nomatch_test);
ATF_TC_HEAD(config_include_nomatch_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test finds no files to include");
}

ATF_TC_BODY(config_include_nomatch_test, tc)
{
	struct flappy_handle xh;
	const char *tcsdir;
	char *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);

	buf = flappy_xasprintf("%s/flappy_nomatch.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/nomatch.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);

	/* should contain no repositories */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 0);
}

ATF_TC(config_include_absolute);
ATF_TC_HEAD(config_include_absolute, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by absolute path");
}

ATF_TC_BODY(config_include_absolute, tc)
{
	struct flappy_handle xh;
	const char *tcsdir;
	char *cfg, *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);

	cfg = flappy_xasprintf("%s/flappy2.d", pwd);
	ATF_REQUIRE_EQ(flappy_mkpath(cfg, 0755), 0);

	buf = flappy_xasprintf("%s/flappy_absolute.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/flappy.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/1.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy2.d/1.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);
	/* should contain one repository defined in 1.include.conf */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 1);
}

ATF_TC(config_include_absolute_glob);
ATF_TC_HEAD(config_include_absolute_glob, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by absolute path with globbing");
}

ATF_TC_BODY(config_include_absolute_glob, tc)
{
	struct flappy_handle xh;
	const char *tcsdir;
	char *cfg, *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);

	cfg = flappy_xasprintf("%s/flappy2.d", pwd);
	ATF_REQUIRE_EQ(flappy_mkpath(cfg, 0755), 0);

	buf = flappy_xasprintf("%s/flappy_absolute_glob.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/flappy.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/1.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy2.d/1.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/2.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy2.d/2.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);
	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 2);
}

ATF_TC(config_masking);
ATF_TC_HEAD(config_masking, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test file masking");
}

ATF_TC_BODY(config_masking, tc)
{
	struct flappy_handle xh;
	const char *tcsdir, *repo;
	char *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);
	ret = snprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.sysconfdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(flappy_mkpath(xh.sysconfdir, 0755), 0);

	buf = flappy_xasprintf("%s/1.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/repo.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = flappy_xasprintf("%s/2.include.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/sys-flappy.d/repo.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 1);

	/* should contain repository=1 */
	ATF_REQUIRE_EQ(flappy_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "1");
}

ATF_TC(config_trim_values);
ATF_TC_HEAD(config_trim_values, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test trimming of values");
}

ATF_TC_BODY(config_trim_values, tc)
{
	struct flappy_handle xh;
	const char *tcsdir, *repo;
	char *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);
	ret = snprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.sysconfdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(flappy_mkpath(xh.sysconfdir, 0755), 0);

	buf = flappy_xasprintf("%s/trim.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/1.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 3);

	/* should contain repository=1 */
	ATF_REQUIRE_EQ(flappy_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "1");
	/* should contain repository=2 */
	ATF_REQUIRE_EQ(flappy_array_get_cstring_nocopy(xh.repositories, 1, &repo), true);
	ATF_REQUIRE_STREQ(repo, "2");
	ATF_REQUIRE_EQ(flappy_array_get_cstring_nocopy(xh.repositories, 2, &repo), true);
	ATF_REQUIRE_STREQ(repo, "3");
}

ATF_TC(config_no_trailing_newline);
ATF_TC_HEAD(config_no_trailing_newline, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test configuration files without trailing newline");
}

ATF_TC_BODY(config_no_trailing_newline, tc)
{
	struct flappy_handle xh;
	const char *tcsdir, *repo;
	char *buf, *buf2, pwd[PATH_MAX];
	int ret;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	flappy_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	flappy_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ret = snprintf(xh.confdir, sizeof(xh.confdir), "%s/flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.confdir)), 1);
	ret = snprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-flappy.d", pwd);
	ATF_REQUIRE_EQ((ret >= 0), 1);
	ATF_REQUIRE_EQ(((size_t)ret < sizeof(xh.sysconfdir)), 1);

	ATF_REQUIRE_EQ(flappy_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(flappy_mkpath(xh.sysconfdir, 0755), 0);

	buf = flappy_xasprintf("%s/no-trailing-nl.cf", tcsdir);
	buf2 = flappy_xasprintf("%s/flappy.d/1.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = FLAPPY_FLAG_DEBUG;
	ATF_REQUIRE_EQ(flappy_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(flappy_array_count(xh.repositories), 1);

	/* should contain repository=test */
	ATF_REQUIRE_EQ(flappy_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "test");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, config_include_test);
	ATF_TP_ADD_TC(tp, config_include_nomatch_test);
	ATF_TP_ADD_TC(tp, config_include_absolute);
	ATF_TP_ADD_TC(tp, config_include_absolute_glob);
	ATF_TP_ADD_TC(tp, config_masking);
	ATF_TP_ADD_TC(tp, config_trim_values);
	ATF_TP_ADD_TC(tp, config_no_trailing_newline);

	return atf_no_error();
}
