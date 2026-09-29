/*-
 * Copyright (c) 2012-2014 Juan Romero Pardines.
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
#include <string.h>
#include <limits.h>

#include <atf-c.h>
#include <flappy.h>

ATF_TC(flappy_path_clean);

ATF_TC_HEAD(flappy_path_clean, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test flappy_path_clean");
}

ATF_TC_BODY(flappy_path_clean, tc)
{
	char buf[PATH_MAX];
	ssize_t len;
#define CLEAN(a, b)                          \
	flappy_strlcpy(buf, a, sizeof (buf));  \
	len = flappy_path_clean(buf);          \
	ATF_CHECK_EQ(len, sizeof (b)-1);     \
	ATF_CHECK_STREQ(buf, b)

	/* Already clean */
	CLEAN("abc", "abc");
	CLEAN("abc/def", "abc/def");
	CLEAN("a/b/c", "a/b/c");
	CLEAN(".", ".");
	CLEAN("..", "..");
	CLEAN("../..", "../..");
	CLEAN("../../abc", "../../abc");
	CLEAN("/abc", "/abc");
	CLEAN("/", "/");

	/* Empty is current dir */
	CLEAN("", ".");

	/* Remove trailing slash */
	CLEAN("abc/", "abc");
	CLEAN("abc/def/", "abc/def");
	CLEAN("a/b/c/", "a/b/c");
	CLEAN("./", ".");
	CLEAN("../", "..");
	CLEAN("../../", "../..");
	CLEAN("/abc/", "/abc");

	/* Remove doubled slash */
	CLEAN("abc//def//ghi", "abc/def/ghi");
	CLEAN("//abc", "/abc");
	CLEAN("///abc", "/abc");
	CLEAN("//abc//", "/abc");
	CLEAN("abc//", "abc");

	/* Remove . elements */
	CLEAN("abc/./def", "abc/def");
	CLEAN("/./abc/def", "/abc/def");
	CLEAN("abc/.", "abc");

	/* Remove .. elements */
	CLEAN("abc/def/ghi/../jkl", "abc/def/jkl");
	CLEAN("abc/def/../ghi/../jkl", "abc/jkl");
	CLEAN("abc/def/..", "abc");
	CLEAN("abc/def/../..", ".");
	CLEAN("/abc/def/../..", "/");
	CLEAN("abc/def/../../..", "..");
	CLEAN("/abc/def/../../..", "/");
	CLEAN("abc/def/../../../ghi/jkl/../../../mno", "../../mno");
	CLEAN("/../abc", "/abc");

	/* Combinations */
	CLEAN("abc/./../def", "def");
	CLEAN("abc//./../def", "def");
	CLEAN("abc/../../././../def", "../../def");

	/* Add test case with "hidden" dir */
	CLEAN("foo//bar/.fizz/buzz", "foo/bar/.fizz/buzz");
	CLEAN(".fizz/buzz", ".fizz/buzz");
	CLEAN(".fizz", ".fizz");
}

ATF_TC(flappy_path_rel);

ATF_TC_HEAD(flappy_path_rel, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test flappy_path_rel");
}

ATF_TC_BODY(flappy_path_rel, tc)
{
	char buf[PATH_MAX];
	ssize_t len;

#define REL(a, b, c) \
	len = flappy_path_rel(buf, sizeof buf, a, b); \
	ATF_CHECK_EQ(len, sizeof (c)-1);            \
	ATF_CHECK_STREQ(buf, c)

	REL("/root/usr/bin/tar", "/root/usr/bin/gtar", "gtar");

	REL("/root/usr/bin/java", "/root/usr/lib/jvm/jdk1.8.0_202/bin/java",
	    "../lib/jvm/jdk1.8.0_202/bin/java");

	REL("/root/usr/..", "/root/usr/lib/..", "root/usr");
	REL("/root/usr/../bin", "/root/usr/lib/..", "usr");
	REL("/root/usr/../bin", "/root/usr/", "usr");

	REL("/root/usr/bin/tar", "/root/usr/libexec/gtar", "../libexec/gtar");
	REL("/root/usr/bin//tar", "/root/usr/libexec/gtar", "../libexec/gtar");
	REL("/root/usr/bin//tar", "/root/usr/libexec//gtar", "../libexec/gtar");
	REL("/usr/bin/file", "/usr/bin/fileA", "fileA");
}

ATF_TC(flappy_path_join);

ATF_TC_HEAD(flappy_path_join, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test flappy_path_join");
}

ATF_TC_BODY(flappy_path_join, tc)
{
	char buf[6];
	ssize_t len;

	len = flappy_path_join(buf, sizeof buf, "a", "b", "c", (char *)NULL);
	ATF_CHECK_EQ(len, 5);
	ATF_CHECK_STREQ(buf, "a/b/c");

	len = flappy_path_join(buf, sizeof buf, "a/", "/b/", "/c", (char *)NULL);
	ATF_CHECK_EQ(len, 5);
	ATF_CHECK_STREQ(buf, "a/b/c");

	len = flappy_path_join(buf, sizeof buf, "abc", "def", (char *)NULL);
	ATF_CHECK_EQ(len, -1);

	len = flappy_path_join(buf, sizeof buf, "abcd", "ef", (char *)NULL);
	ATF_CHECK_EQ(len, -1);

	len = flappy_path_join(buf, sizeof buf, "ab", "c", (char *)NULL);
	ATF_CHECK_EQ(len, 4);
	ATF_CHECK_STREQ(buf, "ab/c");

	len = flappy_path_join(buf, sizeof buf, "ab/", "/c", (char *)NULL);
	ATF_CHECK_EQ(len, 4);
	ATF_CHECK_STREQ(buf, "ab/c");

	len = flappy_path_join(buf, sizeof buf, "/ab/", "/c", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("/ab/c") - 1);
	ATF_CHECK_STREQ(buf, "/ab/c");

	len = flappy_path_join(buf, sizeof buf, "/a/", "/b/", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("/a/b/") - 1);
	ATF_CHECK_STREQ(buf, "/a/b/");

	len = flappy_path_join(buf, sizeof buf, "", "/a/", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("/a/") - 1);
	ATF_CHECK_STREQ(buf, "/a/");

	len = flappy_path_join(buf, sizeof buf, "a", "b/", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("a/b/") - 1);
	ATF_CHECK_STREQ(buf, "a/b/");

	len = flappy_path_join(buf, sizeof buf, "/", "a/", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("/a/") - 1);
	ATF_CHECK_STREQ(buf, "/a/");

	len = flappy_path_join(buf, sizeof buf, "/", "a", (char *)NULL);
	ATF_CHECK_EQ(len, sizeof ("/a") - 1);
	ATF_CHECK_STREQ(buf, "/a");
}

ATF_TC(flappy_path_append);

ATF_TC_HEAD(flappy_path_append, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test flappy_path_append");
}

ATF_TC_BODY(flappy_path_append, tc)
{
	char buf[16];
	ssize_t len;

	/* empty prefix */
	flappy_strlcpy(buf, "fizz", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "");
	ATF_CHECK_EQ(len, sizeof ("fizz") - 1);
	ATF_CHECK_STREQ(buf, "fizz");

	/* empty dst */
	buf[0] = '\0';
	len = flappy_path_append(buf, sizeof buf, "buzz");
	ATF_CHECK_EQ(len, sizeof ("buzz") - 1);
	ATF_CHECK_STREQ(buf, "buzz");

	/* add slash */
	flappy_strlcpy(buf, "fizz", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "buzz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* already has slash in dst */
	flappy_strlcpy(buf, "fizz/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "buzz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* already has slash in suffix */
	flappy_strlcpy(buf, "fizz", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "/buzz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* slash in dst and suffix */
	flappy_strlcpy(buf, "fizz/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "/buzz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	flappy_strlcpy(buf, "abcdefghijklmno", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "pqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklmn", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "opqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklm", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "nopqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklmno/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "pqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklmn/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "opqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklm/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "nopqrstuvwxyz");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "abcdefghijklmno", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklmno") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklmno");

	flappy_strlcpy(buf, "abcdefghijklmn/", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklmn/") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklmn/");

	flappy_strlcpy(buf, "", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "abcdefghijklmno");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklmno") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklmno");

	flappy_strlcpy(buf, "", sizeof buf);
	len = flappy_path_append(buf, sizeof buf, "abcdefghijklmn/");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklmn/") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklmn/");
}

ATF_TC(flappy_path_prepend);

ATF_TC_HEAD(flappy_path_prepend, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test flappy_path_prepend");
}

ATF_TC_BODY(flappy_path_prepend, tc)
{
	char buf[16];
	ssize_t len;

	/* empty prefix */
	flappy_strlcpy(buf, "buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "");
	ATF_CHECK_EQ(len, sizeof ("buzz") - 1);
	ATF_CHECK_STREQ(buf, "buzz");

	/* empty dst */
	buf[0] = '\0';
	len = flappy_path_prepend(buf, sizeof buf, "buzz");
	ATF_CHECK_EQ(len, sizeof ("buzz") - 1);
	ATF_CHECK_STREQ(buf, "buzz");

	/* add slash */
	flappy_strlcpy(buf, "buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* already has slash in dst */
	flappy_strlcpy(buf, "/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* already has slash in prefix */
	flappy_strlcpy(buf, "buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* slash in dst and prefix */
	flappy_strlcpy(buf, "/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/");
	ATF_CHECK_EQ(len, sizeof ("fizz/buzz") - 1);
	ATF_CHECK_STREQ(buf, "fizz/buzz");

	/* check truncation no slashes */
	flappy_strlcpy(buf, "bar/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/foo");
	ATF_CHECK_EQ(len, -1);

	/* check truncation slash in dst*/
	flappy_strlcpy(buf, "/bar/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/foo");
	ATF_CHECK_EQ(len, -1);

	/* check truncation slash in prefix */
	flappy_strlcpy(buf, "bar/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/foo/");
	ATF_CHECK_EQ(len, -1);

	/* check truncation slash in both */
	flappy_strlcpy(buf, "/bar/buzz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "fizz/foo/");
	ATF_CHECK_EQ(len, -1);

	/* check truncation */
	flappy_strlcpy(buf, "pqrstuvwxyz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklmno");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "/opqrstuvwxyz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklmn/");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "/nopqrstuvwxyz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklm/");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "opqrstuvwxyz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklmn");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "nopqrstuvwxyz", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklm");
	ATF_CHECK_EQ(len, -1);

	flappy_strlcpy(buf, "", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklmno");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklmno") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklmno");

	flappy_strlcpy(buf, "", sizeof buf);
	len = flappy_path_prepend(buf, sizeof buf, "abcdefghijklm/");
	ATF_CHECK_EQ(len, sizeof ("abcdefghijklm/") - 1);
	ATF_CHECK_STREQ(buf, "abcdefghijklm/");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, flappy_path_clean);
	ATF_TP_ADD_TC(tp, flappy_path_rel);
	ATF_TP_ADD_TC(tp, flappy_path_join);
	ATF_TP_ADD_TC(tp, flappy_path_append);
	ATF_TP_ADD_TC(tp, flappy_path_prepend);
	return atf_no_error();
}
