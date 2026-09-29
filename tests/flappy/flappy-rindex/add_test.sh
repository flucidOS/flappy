#! /usr/bin/env atf-sh
# Test that flappy-rindex(1) -a (add mode) works as expected.

# 1st test: test that update mode work as expected.
atf_test_case update

update_head() {
	atf_set "descr" "flappy-rindex(1) -a: update test"
}

update_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	flappy-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	flappy-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	result="$(flappy-query -r root -C empty.conf --repository=some_repo -s '')"
	expected="[-] foo-1.1_1 foo pkg"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case revert

revert_head() {
	atf_set "descr" "flappy-rindex(1) -a: revert version test"
}

revert_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	atf_check -o ignore -e ignore -- flappy-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check -o ignore -e ignore -- flappy-rindex -d -a $PWD/*.flappy
	atf_check -o ignore -e ignore -- flappy-create -A noarch -n foo-1.0_1 -r "1.1_1" -s "foo pkg" ../pkg_A
	atf_check -o ignore -e ignore -- flappy-rindex -d -a $PWD/*.flappy
	cd ..
	atf_check -o "inline:[-] foo-1.0_1 foo pkg\n" -e empty -- \
		flappy-query -r root -C empty.conf --repository=some_repo -Rs ''
}

atf_test_case stage

stage_head() {
	atf_set "descr" "flappy-rindex(1) -a: commit package to stage test"
}

stage_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00 pkg_B/file01
	cd some_repo
	flappy-create -A noarch -n foo-1.0_1 -s "foo pkg" --shlib-provides "libfoo.so.1" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    1 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	flappy-create -A noarch -n foo-1.1_1 -s "foo pkg" --shlib-provides "libfoo.so.2" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    1 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	flappy-create -A noarch -n bar-1.0_1 -s "foo pkg" --shlib-requires "libfoo.so.2" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    2 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	flappy-create -A noarch -n foo-1.2_1 -s "foo pkg" --shlib-provides "libfoo.so.3" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    2 $PWD (Staged) (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	flappy-create -A noarch -n bar-1.1_1 -s "foo pkg" --shlib-requires "libfoo.so.3" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    2 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L
}

atf_test_case stage_resolve_bug

stage_resolve_bug_head() {
	atf_set "descr" "flappy-rindex(1) -a: commit package to stage test"
}

stage_resolve_bug_body() {
	# Scanario: provides a shlib, then it is moved (for example for splitting the
	# pkg) to libprovider. afterwards requirer is added resulting in
	mkdir provider libprovider requirer stage-trigger some_repo
	touch provider/1 libprovider/2 requirer/3 stage-trigger/4
	cd some_repo

	# first add the provider and the requirer to the repo
	flappy-create -A noarch -n provider-1.0_1 -s "foo pkg" --shlib-provides "libfoo.so.1 libbar.so.1" ../provider
	atf_check_equal $? 0
	flappy-create -A noarch -n require-1.0_1 -s "foo pkg" --shlib-requires "libfoo.so.1" ../requirer
	atf_check_equal $? 0
	flappy-create -A noarch -n stage-trigger-1.0_1 -s "foo pkg" --shlib-requires "libbar.so.1" ../stage-trigger
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	# then add libprovider that also provides the library
	flappy-create -A noarch -n libprovider-1.0_2 -s "foo pkg" --shlib-provides "libfoo.so.1" ../libprovider
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    4 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	# trigger staging
	flappy-create -A noarch -n provider-1.0_2 -s "foo pkg" --shlib-provides "libfoo.so.1" ../provider
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    4 $PWD (Staged) (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	# then add a new provider not containing the provides field. This resulted in
	# a stage state despites the library is resolved through libprovides
	flappy-create -A noarch -n provider-1.0_3 -s "foo pkg" ../provider
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    4 $PWD (Staged) (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L

	# resolve staging
	# the actual bug appeared here: libfoo.so.1 is still provided by libprovider, but
	# flappy-rindex fails to register that.
	flappy-create -A noarch -n stage-trigger-1.0_2 -s "foo pkg" ../stage-trigger
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	atf_check -o inline:"    4 $PWD (RSA unsigned)\n" -- \
		flappy-query -r ../root -i --repository=$PWD -L
}

atf_test_case stage_stacked

stage_stacked_head() {
	atf_set "descr" "flappy-rindex(1) -a: staging multiple libraries and clean one to half unstage"
}

stage_stacked_body() {
	mkdir -p repo root pkg

	cd repo
	atf_check -o ignore -- flappy-create -A noarch -n flac-1.4.3_1 -s "flac pkg" --shlib-provides "libFLAC.so.12" ../pkg
	atf_check -o ignore -- flappy-create -A noarch -n ruby-3.3.8_1 -s "ruby pkg" --shlib-provides "libruby.so.3.3" ../pkg
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" --shlib-requires "libFLAC.so.12" ../pkg
	atf_check -o ignore -- flappy-create -A noarch -n B-1.0_1 -s "B pkg" --shlib-requires  "libruby.so.3.3" ../pkg
	atf_check -e ignore -o ignore -- flappy-rindex -va *.flappy

	atf_check -o ignore -- flappy-create -A noarch -n flac-1.5.0_1 -s "flac pkg" --shlib-provides "libFLAC.so.14" ../pkg
	atf_check -e ignore -o match:"stage: added \`flac-1\.5\.0_1' \(noarch\)" -- flappy-rindex -va flac-1.5.0_1.noarch.flappy

	atf_check -o ignore -- flappy-create -A noarch -n ruby-3.4.5_1 -s "ruby pkg" --shlib-provides "libruby.so.3.4" ../pkg
	atf_check -e ignore -o match:"stage: added \`ruby-3\.4\.5_1' \(noarch\)" -- flappy-rindex -va ruby-3.4.5_1.noarch.flappy

	# atf_check -o ignore -- flappy-create -A noarch -n A-1.0_2 -s "A pkg" --shlib-requires  "libFLAC.so.14" ../pkg
	# atf_check -e ignore -o match:"stage: added \`A-1\.0_2' \(noarch\)" -- flappy-rindex -va A-1.0_2.noarch.flappy

	rm A-1.0_1.noarch.flappy
	atf_check -o match:"index: removed pkg A-1\.0_1" -- flappy-rindex -c .

	atf_check -o match:"\(Staged\)" -- flappy-query -r root --repository=. -L

	atf_check -o ignore -- flappy-create -A noarch -n C-1.0_1 -s "C pkg" --shlib-requires  "libFLAC.so.14" ../pkg
	atf_check -o match:"stage: added \`C-1\.0_1' \(noarch\)" -- flappy-rindex -va C-1.0_1.noarch.flappy

	atf_check -o ignore -- flappy-create -A noarch -n B-1.0_2 -s "B pkg" --shlib-requires  "libruby.so.3.4" ../pkg
	atf_check -o match:"index: added \`B-1\.0_2' \(noarch\)" -- flappy-rindex -va B-1.0_2.noarch.flappy

}

atf_init_test_cases() {
	atf_add_test_case update
	atf_add_test_case revert
	atf_add_test_case stage
	atf_add_test_case stage_resolve_bug
	atf_add_test_case stage_stacked
}
