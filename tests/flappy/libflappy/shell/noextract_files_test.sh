#!/usr/bin/env atf-sh

atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for pkg install with noextract: match whole directory"
}

tc1_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/lib
	touch pkg_A/usr/bin/blah pkg_A/usr/bin/foo pkg_A/usr/lib/foo
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "noextract=/usr/bin/*" > root/flappy.d/foo.conf

	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/usr/lib/foo ] || rv=1
	[ -e root/usr/bin/blah ] && rv=1
	[ -e root/usr/bin/foo ] && rv=1
	atf_check_equal $rv 0

	flappy-pkgdb -C flappy.d -r root A
	atf_check_equal $? 0
}

atf_test_case tc2

tc2_head() {
	atf_set "descr" "Tests for pkg install with noextract: match certain file"
}

tc2_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/lib
	touch pkg_A/usr/bin/blah pkg_A/usr/bin/foo pkg_A/usr/lib/foo
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "noextract=/usr/bin/f*" > root/flappy.d/foo.conf

	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	tree root
	rv=0
	[ -e root/usr/lib/foo ] || rv=1
	[ -e root/usr/bin/blah ] || rv=1
	[ -e root/usr/bin/foo ] && rv=1
	atf_check_equal $rv 0

	flappy-pkgdb -C flappy.d -r root A
	atf_check_equal $? 0
}

atf_test_case tc3

tc3_head() {
	atf_set "descr" "Tests for pkg install with noextract: negate pattern"
}

tc3_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/lib
	touch pkg_A/usr/bin/blah pkg_A/usr/bin/foo pkg_A/usr/lib/foo
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "noextract=/usr/bin/*" > root/flappy.d/foo.conf
	echo "noextract=!/usr/bin/blah" >> root/flappy.d/foo.conf

	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	tree root
	rv=0
	[ -e root/usr/lib/foo ] || rv=1
	[ -e root/usr/bin/blah ] || rv=1
	[ -e root/usr/bin/foo ] && rv=1
	atf_check_equal $rv 0

	flappy-pkgdb -C flappy.d -r root A
	atf_check_equal $? 0
}

tc4_head() {
	atf_set "descr" "Tests for pkg install with noextract: negate and match again"
}

tc4_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/lib
	touch pkg_A/usr/bin/blah pkg_A/usr/bin/foo pkg_A/usr/lib/foo
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "noextract=/usr/bin/*" > root/flappy.d/foo.conf
	echo "noextract=!/usr/bin/blah" >> root/flappy.d/foo.conf
	echo "noextract=/usr/bin/bla*" >> root/flappy.d/foo.conf

	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	tree root
	rv=0
	[ -e root/usr/lib/foo ] || rv=1
	[ -e root/usr/bin/blah ] && rv=1
	[ -e root/usr/bin/foo ] && rv=1
	atf_check_equal $rv 0

	flappy-pkgdb -C flappy.d -r root A
	atf_check_equal $? 0
}

atf_test_case tc5

tc5_head() {
	atf_set "descr" "Tests for pkg install with noextract: match full path"
}

tc5_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_A/usr/lib
	touch pkg_A/usr/bin/blah pkg_A/usr/bin/foo pkg_A/usr/lib/foo
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "noextract=*foo" > root/flappy.d/foo.conf

	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=0
	[ -e root/usr/lib/foo ] && rv=1
	[ -e root/usr/bin/foo ] && rv=1
	[ -e root/usr/bin/blah ] || rv=2
	atf_check_equal $rv 0

	flappy-pkgdb -C flappy.d -r root A
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case tc1
	atf_add_test_case tc2
	atf_add_test_case tc3
	atf_add_test_case tc4
	atf_add_test_case tc5
}
