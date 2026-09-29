#!/usr/bin/env atf-sh

atf_test_case update_flappy

update_flappy_head() {
	atf_set "descr" "Tests for pkg updates: flappy autoupdates itself"
}

update_flappy_body() {
	mkdir -p repo flappy
	touch flappy/foo

	cd repo
	flappy-create -A noarch -n flappy-1.0_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.0_1

	cd repo
	flappy-create -A noarch -n flappy-1.1_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/flappy-1.1_1.noarch.flappy
	atf_check_equal $? 0
	cd ..

	# EBUSY
	flappy-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 16

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.0_1

	flappy-install -r root --repository=$PWD/repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.1_1
}

atf_test_case update_flappy_with_revdeps

update_flappy_with_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: flappy updates itself with revdeps"
}

update_flappy_with_revdeps_body() {
	mkdir -p repo flappy flappy-dbg baz
	touch flappy/foo flappy-dbg/bar baz/blah

	cd repo
	flappy-create -A noarch -n flappy-1.0_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd flappy-1.0_1
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n baz-1.0_1 -s "baz pkg" ../baz
	atf_check_equal $? 0
	flappy-create -A noarch -n flappy-dbg-1.0_1 -s "flappy-dbg pkg" --dependencies "flappy-1.0_1" ../flappy-dbg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd flappy-dbg baz
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n flappy-1.1_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-create -A noarch -n baz-1.1_1 -s "baz pkg" ../baz
	atf_check_equal $? 0
	flappy-create -A noarch -n flappy-dbg-1.1_1 -s "flappy-dbg pkg" --dependencies "flappy-1.1_1" ../flappy-dbg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	# first time, flappy must be updated (returns EBUSY)
	flappy-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 16

	flappy-install -r root --repository=$PWD/repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.1_1

	out=$(flappy-query -r root -p pkgver flappy-dbg)
	atf_check_equal $out flappy-dbg-1.1_1

	out=$(flappy-query -r root -p pkgver baz)
	atf_check_equal $out baz-1.0_1

	# second time, updates everything
	flappy-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.1_1

	out=$(flappy-query -r root -p pkgver flappy-dbg)
	atf_check_equal $out flappy-dbg-1.1_1

	out=$(flappy-query -r root -p pkgver baz)
	atf_check_equal $out baz-1.1_1
}

atf_test_case update_flappy_with_uptodate_revdeps

update_flappy_with_uptodate_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: flappy updates itself with already up-to-date revdeps"
}

update_flappy_with_uptodate_revdeps_body() {
	mkdir -p repo flappy base-system
	touch flappy/foo base-system/bar

	cd repo
	flappy-create -A noarch -n flappy-1.0_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-create -A noarch -n base-system-1.0_1 -s "base-system pkg" --dependencies "flappy>=0" ../base-system
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd base-system
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out "flappy-1.0_1"

	out=$(flappy-query -r root -p pkgver base-system)
	atf_check_equal $out "base-system-1.0_1"

	cd repo
	flappy-create -A noarch -n flappy-1.1_1 -s "flappy pkg" ../flappy
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yud
	atf_check_equal $? 16

	flappy-install -r root --repository=$PWD/repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal $out flappy-1.1_1

	out=$(flappy-query -r root -p pkgver base-system)
	atf_check_equal $out base-system-1.0_1
}

atf_test_case update_flappy_with_indirect_revdeps

update_flappy_with_indirect_revdeps_head() {
	atf_set "descr" "Tests for pkg updates: flappy updates itself with indirect revdeps"
}

update_flappy_with_indirect_revdeps_body() {
	mkdir -p repo pkg

	cd repo
	flappy-create -A noarch -n flappy-1.0_1 -s "flappy pkg" --dependencies "libcrypto-1.0_1 cacerts>=0" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n libcrypto-1.0_1 -s "libcrypto pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n libressl-1.0_1 -s "libressl pkg" --dependencies "libcrypto-1.0_1" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n cacerts-1.0_1 -s "cacerts pkg" --dependencies "libressl>=0" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd flappy-1.0_1
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal "$out" "flappy-1.0_1"

	out=$(flappy-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.0_1"

	out=$(flappy-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.0_1"

	out=$(flappy-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"

	cd repo
	flappy-create -A noarch -n flappy-1.1_1 -s "flappy pkg" --dependencies "libcrypto-1.1_1 ca-certs>=0" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n libcrypto-1.1_1 -s "libcrypto pkg" ../pkg
	atf_check_equal $? 0
	flappy-create -A noarch -n libressl-1.1_1 -s "libressl pkg" --dependencies "libcrypto-1.1_1" ../pkg
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yu flappy
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal "$out" "flappy-1.1_1"

	out=$(flappy-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.1_1"

	out=$(flappy-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.0_1"

	out=$(flappy-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"

	flappy-install -r root --repository=$PWD/repo -yu
	atf_check_equal $? 0

	out=$(flappy-query -r root -p pkgver flappy)
	atf_check_equal "$out" "flappy-1.1_1"

	out=$(flappy-query -r root -p pkgver libcrypto)
	atf_check_equal "$out" "libcrypto-1.1_1"

	out=$(flappy-query -r root -p pkgver libressl)
	atf_check_equal "$out" "libressl-1.1_1"

	out=$(flappy-query -r root -p pkgver cacerts)
	atf_check_equal "$out" "cacerts-1.0_1"
}

atf_init_test_cases() {
	atf_add_test_case update_flappy
	atf_add_test_case update_flappy_with_revdeps
	atf_add_test_case update_flappy_with_indirect_revdeps
	atf_add_test_case update_flappy_with_uptodate_revdeps
}
