#! /usr/bin/env atf-sh
# Test that flappy-query(1) -i works as expected

atf_test_case ignore_system

ignore_system_head() {
	atf_set "descr" "flappy-query(1) -i: ignore repos defined in the system directory (sharedir/flappy.d)"
}

ignore_system_body() {
	mkdir -p repo pkg_A/bin
	touch pkg_A/bin/file
	ln -s repo repo1
	cd repo
	flappy-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	rm -f *.flappy
	cd ..
	systemdir=$(flappy-uhelper getsystemdir)
	mkdir -p root/${systemdir}
	echo "repository=$PWD/repo1" > root/${systemdir}/myrepo.conf
	out="$(flappy-query -C empty.conf --repository=$PWD/repo -i -L|wc -l)"
	atf_check_equal "$out" 1
}

atf_test_case ignore_conf

ignore_conf_head() {
	atf_set "descr" "flappy-query(1) -i: ignore repos defined in the configuration directory (flappy.d)"
}

ignore_conf_body() {
	mkdir -p repo pkg_A/bin
	touch pkg_A/bin/file
	ln -s repo repo1
	cd repo
	flappy-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	rm -f *.flappy
	cd ..
	mkdir -p root/flappy.d
	echo "repository=$PWD/repo1" > root/flappy.d/myrepo.conf
	out="$(flappy-query -r root -C flappy.d --repository=$PWD/repo -i -L|wc -l)"
	atf_check_equal "$out" 1
}

atf_init_test_cases() {
	atf_add_test_case ignore_conf
	atf_add_test_case ignore_system
}
