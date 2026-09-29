#! /usr/bin/env atf-sh

atf_test_case revert_package

revert_package_head() {
	atf_set "descr" "flappy-install(1): do a full workflow of reverting a package version"
}

revert_package_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	# create package and install it
	cd some_repo
	echo first V1.0 > ../pkg_A/file00
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0

	# make an update to the package
	cd some_repo
	echo V1.1 > ../pkg_A/file00
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	# whoops, we made a mistake, rollback to 1.0_1
	cd some_repo
	echo second V1.0 > ../pkg_A/file00
	flappy-create -A noarch -n A-1.0_1 -r "1.1_1" -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -d -r root -C empty.conf --repository=$PWD/some_repo -y -u
	atf_check_equal $? 0

	atf_check_equal "`cat root/file00`" "second V1.0"
}

atf_init_test_cases() {
	atf_add_test_case revert_package
}
