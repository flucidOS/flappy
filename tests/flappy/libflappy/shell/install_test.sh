#!/usr/bin/env atf-sh

atf_test_case install_empty

install_empty_head() {
	atf_set "descr" "Tests for pkg installations: install pkg with no files"
}

install_empty_body() {
	mkdir -p repo pkg_A
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -C empty.conf -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	rv=0
	if [ -e root/var/db/flappy/.A-files.plist ]; then
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case install_with_deps

install_with_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with deps in proper order"
}

install_with_deps_body() {
	# Proper order: A B C D
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "A>=0 B>=0" ../pkg_C
	atf_check_equal $? 0
	flappy-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0 B>=0" ../pkg_D
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	printf "A-1.0_1\nB-1.0_1\nC-1.0_1\nD-1.0_1\n" > exp
	flappy-install -C empty.conf -r root --repository=$PWD/some_repo -yn D|awk '{print $1}' > out
	echo "exp: '$(cat exp)'" >&2
	echo "out: '$(cat out)'" >&2
	cmp exp out
	atf_check_equal $? 0
}

atf_test_case install_with_vpkg_deps

install_with_vpkg_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with virtual pkg updates in proper order"
}

install_with_vpkg_deps_body() {
	# Proper order: D C A B
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --provides="E-1.0_1" --dependencies "D>=0" ../pkg_C
	atf_check_equal $? 0
	flappy-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "E>=0 B>=0" ../pkg_D
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	printf "A-1.0_1\nB-1.0_1\nD-1.0_1\nC-1.0_1\n" > exp
	flappy-install -C empty.conf -r root --repository=$PWD/some_repo -yn E|awk '{print $1}' > out
	echo "exp: '$(cat exp)'" >&2
	echo "out: '$(cat out)'" >&2
	cmp exp out
	atf_check_equal $? 0
}

atf_test_case update_to_empty_pkg

update_to_empty_pkg_head() {
	atf_set "descr" "Tests for pkg installations: update pkg with files to another version without"
}

update_to_empty_pkg_body() {
	mkdir -p repo pkg_A/usr/bin
	touch pkg_A/usr/bin/blah
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -C empty.conf -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0

	cd repo
	rm -rf ../pkg_A/*
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -C empty.conf -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0
	rv=0
	if [ -e root/usr/bin/blah -a -e root/var/db/flappy/.A-files.plist ]; then
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case update_if_installed

update_if_installed_head() {
	atf_set "descr" "Tests for pkg installations: update if installed (issue #35)"
}

update_if_installed_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	cd some_repo
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	pkgver=$(flappy-query -r root -ppkgver A)
	atf_check_equal $pkgver A-1.1_1
}

atf_test_case install_if_not_installed_on_update

install_if_not_installed_on_update_head() {
	atf_set "descr" "Tests for pkg installations: install if not installed on update (issue #35)"
}

install_if_not_installed_on_update_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -C empty.conf -r root --repository=$PWD/some_repo -yu A
	atf_check_equal $? 0

	pkgver=$(flappy-query -r root -ppkgver A)
	atf_check_equal $pkgver A-1.0_1
}

atf_test_case install_dups

install_dups_head() {
	atf_set "descr" "Tests for pkg installations: install multiple times a pkg"
}

install_dups_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	out=$(flappy-install -C empty.conf -r root --repository=$PWD/some_repo -ynd A A A A|wc -l)
	atf_check_equal $out 1
}

atf_test_case install_bestmatch

install_bestmatch_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching enabled"
}

install_bestmatch_body() {
	mkdir -p repo repo2 pkg_A/usr/bin
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ../repo2
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	mkdir -p root/flappy.d
	echo "bestmatching=true" > root/flappy.d/bestmatch.conf
	flappy-install -C flappy.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd A
	atf_check_equal $? 0
	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
}

atf_test_case install_bestmatch_deps

install_bestmatch_deps_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching enabled for deps"
}

install_bestmatch_deps_body() {
	mkdir -p repo repo2 pkg_A/usr/bin pkg_B/usr/bin
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ../repo2
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	mkdir -p root/flappy.d
	echo "bestmatching=true" > root/flappy.d/bestmatch.conf
	flappy-install -C flappy.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd B
	atf_check_equal $? 0
	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
	out=$(flappy-query -r root -p pkgver B)
	atf_check_equal $out B-1.1_1
}

atf_test_case install_bestmatch_disabled

install_bestmatch_disabled_head() {
	atf_set "descr" "Tests for pkg installations: install with bestmatching disabled"
}

install_bestmatch_disabled_body() {
	mkdir -p repo repo2 pkg_A/usr/bin
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ../repo2
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	mkdir -p root/flappy.d
	echo "bestmatching=false" > root/flappy.d/bestmatch.conf
	flappy-install -C flappy.d -r root --repository=$PWD/repo --repository=$PWD/repo2 -yvd A
	atf_check_equal $? 0
	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1
}

atf_test_case install_and_update_revdeps

install_and_update_revdeps_head() {
	atf_set "descr" "Tests for pkg install: install pkg and update its revdeps"
}

install_and_update_revdeps_body() {
	mkdir -p repo pkg/usr/bin
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A-1.0_1" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B-1.0_1" ../pkg
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	flappy-install -r root --repository=repo -yvd C
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.0_1
	atf_check_equal $(flappy-query -r root -p pkgver B) B-1.0_1
	atf_check_equal $(flappy-query -r root -p pkgver C) C-1.0_1

	cd repo
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_2 -s "B pkg" --dependencies "A-1.0_2" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_2 -s "C pkg" --dependencies "B-1.0_2" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C-1.0_2" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root --repository=repo -yvd D
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.0_2
	atf_check_equal $(flappy-query -r root -p pkgver B) B-1.0_2
	atf_check_equal $(flappy-query -r root -p pkgver C) C-1.0_2
	atf_check_equal $(flappy-query -r root -p pkgver D) D-1.0_1
}

atf_test_case install_virtual_already_installed

install_virtual_already_installed_head() {
	atf_set "descr" "Tests for pkg install: do not reinstall an already installed virtual package"
}

install_virtual_already_installed_body() {
	mkdir -p repo1 repo1-dbg repo2 pkg/usr/bin

	cd repo1
	touch ../pkg/usr/bin/A
	atf_check -o ignore -e ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "cmd:A-1.0_1" ../pkg
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..

	atf_check -o match:'A-1\.0_1: installed successfully\.' -- \
		flappy-install -r root -R repo1 -yv A

	atf_check -o not-match:'A-1\.0_1: installed successfully\.' \
		-e inline:"ERROR: Package \`cmd:A' already installed.\n" -- \
		flappy-install -r root -R repo1 -yv cmd:A
}

atf_test_case install_virtual_already_installed_as_dep

install_virtual_already_installed_as_dep_head() {
	atf_set "descr" "Tests for pkg install: do not reinstall an already installed virtual package as dependency"
}

install_virtual_already_installed_as_dep_body() {
	mkdir -p repo1 pkg_A/usr/bin pkg_B/usr/bin

	cd repo1
	touch ../pkg_A/usr/bin/A
	touch ../pkg_B/usr/bin/B
	atf_check -o ignore -e ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "cmd:A-1.0_1" ../pkg_A
	atf_check -o ignore -e ignore -- flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "cmd:A-1.0_1" ../pkg_B
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..

	atf_check -o match:'A-1\.0_1: installed successfully\.' -- \
		flappy-install -r root -R repo1 -yv A

	atf_check -o not-match:'A-1\.0_1: installed successfully\.' -- \
		flappy-install -r root -R repo1 -yv B
}

atf_test_case update_file_timestamps

update_file_timestamps_head() {
	atf_set "descr" "Test for pkg updates: update file timestamps"
}

update_file_timestamps_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	echo 123456789 > pkg_A/usr/include/gsm/gsm.h
	touch -mt 197001010000.00 pkg_A/usr/include/gsm/gsm.h

	cd repo
	flappy-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yvd foo

	expected=$(stat --printf='%Y' pkg_A/usr/include/gsm/gsm.h)
	result=$(stat --printf='%Y' root/usr/include/gsm/gsm.h)

	atf_check_equal "$expected" "$result"

	cd repo
	touch -f pkg_A/usr/include/gsm/gsm.h
	flappy-create -A noarch -n foo-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yuvd foo

	expected=$(stat --printf='%Y' pkg_A/usr/include/gsm/gsm.h)
	result=$(stat --printf='%Y' root/usr/include/gsm/gsm.h)

	atf_check_equal "$expected" "$result"
}

atf_test_case update_move_unmodified_file

update_move_unmodified_file_head() {
	atf_set "descr" "Test for pkg updates: move an unmodified file between pkgs"
}

update_move_unmodified_file_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	touch pkg_A/usr/bin/foo
	echo 123456789 > pkg_B/usr/bin/sg

	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yvd A
	cd repo
	mv ../pkg_B/usr/bin/sg ../pkg_A/usr/bin
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A>=1.1" ../pkg_B
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yuvd B
	atf_check_equal $? 0
	flappy-pkgdb -r root -av
	atf_check_equal $? 0
}

atf_test_case update_move_file

update_move_file_head() {
	atf_set "descr" "Test for pkg updates: move a symlink to a file between pkgs"
}

update_move_file_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin
	echo 987654321 > pkg_A/usr/bin/newgrp
	ln -s /usr/bin/newgrp pkg_B/usr/bin/sg
	touch -mt 197001010000.00 pkg_A/usr/bin/newgrp
	touch -mt 197001010000.00 pkg_B/usr/bin/sg

	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yvd A B
	cd repo
	rm ../pkg_B/usr/bin/sg
	rm ../pkg_A/usr/bin/newgrp
	echo 123456789 > ../pkg_A/usr/bin/sg
	touch -mt 197001010000.00 ../pkg_A/usr/bin/sg
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yuvd
	atf_check_equal $? 0
	flappy-pkgdb -r root -av
	atf_check_equal $? 0

}

atf_test_case update_flappy

update_flappy_head() {
	atf_set "descr" "Test for pkg updates: update flappy if there's an update"
}

update_flappy_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin

	cd repo
	flappy-create -A noarch -n flappy-1.0_1 -s "flappy pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_C
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yvd flappy C
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n flappy-1.1_1 -s "flappy pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.1_1 -s "C pkg" --dependencies "B>=1.1" ../pkg_C
	atf_check_equal $? 0

	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	out=$(flappy-install -r root --repository=repo -yun)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "flappy-1.1_1 update noarch $(readlink -f repo)"

	flappy-install -r root --repository=repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal "$out" "flappy-1.1_1"

	flappy-install -r root --repository=repo -yu
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver B)
	atf_check_equal "$out" "B-1.1_1"
	out=$(flappy-query -r root -p pkgver C)
	atf_check_equal "$out" "C-1.1_1"
}

atf_test_case update_flappy_virtual

update_flappy_virtual_head() {
	atf_set "descr" "Test for pkg updates: update flappy if there's an update (virtual pkg)"
}

update_flappy_virtual_body() {
	mkdir -p repo pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin

	cd repo
	flappy-create -A noarch -n flappy-git-1.0_1 -s "flappy pkg" --provides "flappy-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_C
	atf_check_equal $? 0
	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	flappy-install -r root --repository=repo -yvd flappy C
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n flappy-git-1.1_1 -s "flappy pkg" --provides "flappy-1.1_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.1_1 -s "C pkg" --dependencies "B>=1.1" ../pkg_C
	atf_check_equal $? 0

	cd ..
	flappy-rindex -d -a repo/*.flappy
	atf_check_equal $? 0
	out=$(flappy-install -r root --repository=repo -yun)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "flappy-git-1.1_1 update noarch $(readlink -f repo)"

	flappy-install -r root --repository=repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy-git)
	atf_check_equal "$out" "flappy-git-1.1_1"
}

atf_test_case update_with_revdeps

update_with_revdeps_head() {
	atf_set "descr" "Tests for pkg install: update pkg and its revdeps"
}

update_with_revdeps_body() {
	mkdir -p repo pkg/usr/bin
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_2 -s "B pkg" --dependencies "A-1.0_2" ../pkg
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	flappy-install -r root --repository=repo -yvd A
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.0_1

	cd repo
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root --repository=repo -yvd B
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.0_2
	atf_check_equal $(flappy-query -r root -p pkgver B) B-1.0_2

	cd repo
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" --dependencies "A-1.1_1" ../pkg
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	flappy-install -r root --repository=repo -yvdu A
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.1_1
	atf_check_equal $(flappy-query -r root -p pkgver B) B-1.1_1
}

atf_test_case update_and_install

update_and_install_head() {
	atf_set "descr" "Tests for pkg install: update installed version and install new from other repo"
}

update_and_install_body() {
	mkdir -p repo1 repo1-dbg repo2 pkg/usr/bin

	cd repo1
	touch ../pkg/usr/bin/A
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg
	rm ../pkg/usr/bin/A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	cd repo1-dbg
	flappy-create -A noarch -n A-dbg-1.0_1 -D A-1.0_1 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repo=repo1 --repo=repo1-dbg -yvd A A-dbg
	atf_check_equal $? 0
	atf_check_equal $(flappy-query -r root -p pkgver A) A-1.0_1

	cd repo1
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	cd repo1-dbg
	flappy-create -A noarch -n A-dbg-1.0_2 -D A-1.0_2 -s "A pkg" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	cd repo2
	touch ../pkg/usr/bin/B
	flappy-create -A noarch -n A-2.0_1 -s "A pkg" ../pkg
	rm ../pkg/usr/bin/B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	# Due to first repo wins, returns 19 because can't satisfy revdeps
	flappy-install -r root --repo=repo2 --repo=repo1 --repo=repo1-dbg -ydun A
	atf_check_equal $? 19

	# Try with proper repo ordering
	flappy-install -r root --repo=repo1 --repo=repo1-dbg --repo=repo2 -ydu A
	atf_check_equal $? 0

	out=$(flappy-query -r root -l|wc -l)
	atf_check_equal "$out" "2"

	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal "$out" "A-1.0_2"

	out=$(flappy-query -r root -p pkgver A-dbg)
	atf_check_equal "$out" "A-dbg-1.0_2"

	# check again with A-2.0_1 in first repo.
	flappy-install -r root --repo=repo2 --repo=repo1 --repo=repo1-dbg -ydun A
	atf_check_equal $? 19
}

atf_test_case update_issue_218

update_issue_218_head() {
	atf_set "descr" "Tests for pkg update: issue https://github.com/void-linux/xbps/issues/218"
}

update_issue_218_body() {
	mkdir -p repo pkg

	cd repo
	flappy-create -A noarch -n libGL-1.0_1 -s "libGL" -D "libglapi-1.0_1" ../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n libEGL-1.0_1 -s "libEGL" -D "libglapi-1.0_1 libgbm-1.0_1" ../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n libgbm-1.0_1 -s "libgbm" ../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n libglapi-1.0_1 -s "libglapi" ../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n xorg-server-1.0_1 -s "xorg-server" -D "libgbm>=1.0_1 libGL>=1.0_1" ../pkg
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repo=repo -yd xorg-server
	atf_check_equal $? 0

	cd repo
	rm -f *.flappy

	# provides="libGL-7.11_1 libEGL-7.11_1 libGLES-7.11_1"
	# replaces="libGL>=0 libEGL>=0 libGLES>=0"
	flappy-create -A noarch -n libglvnd-2.0_1 -s "libglvnd" \
		-P "libGL-7.11_1 libEGL-7.11_1 libGLES-7.11_1" \
		-R "libGL>=0 libEGL>=0 libGLES>=0" \
		../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n libgbm-2.0_1 -s "libgbm" ../pkg
	atf_check_equal $? 0

	flappy-create -A noarch -n libglapi-2.0_1 -s "libglapi" -D "libglvnd>=0" ../pkg
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repo=repo -ydvu
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case install_empty
	atf_add_test_case install_with_deps
	atf_add_test_case install_with_vpkg_deps
	atf_add_test_case install_if_not_installed_on_update
	atf_add_test_case install_dups
	atf_add_test_case install_bestmatch
	atf_add_test_case install_bestmatch_deps
	atf_add_test_case install_bestmatch_disabled
	atf_add_test_case install_and_update_revdeps
	atf_add_test_case install_virtual_already_installed
	atf_add_test_case install_virtual_already_installed_as_dep
	atf_add_test_case update_and_install
	atf_add_test_case update_if_installed
	atf_add_test_case update_to_empty_pkg
	atf_add_test_case update_file_timestamps
	atf_add_test_case update_move_file
	atf_add_test_case update_move_unmodified_file
	atf_add_test_case update_flappy
	atf_add_test_case update_flappy_virtual
	atf_add_test_case update_with_revdeps
	atf_add_test_case update_issue_218
}
