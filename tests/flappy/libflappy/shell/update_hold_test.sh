#!/usr/bin/env atf-sh

atf_test_case update_hold

update_hold_head() {
	atf_set "descr" "Tests for pkg update: pkg is on hold mode"
}

update_hold_body() {
	mkdir -p repo pkg_A
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	flappy-pkgdb -r root -m hold A
	atf_check_equal $? 0
	out=$(flappy-query -r root -H)
	atf_check_equal $out A-1.0_1
	cd repo
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1

}

atf_test_case update_pkg_with_held_dep

update_pkg_with_held_dep_head() {
	atf_set "descr" "flappy-install(1): update packages with held dependency (issue #143)"
}

update_pkg_with_held_dep_body() {
	mkdir -p some_repo pkginst pkgheld pkgdep-21_1 pkgdep-22_1
	touch pkginst/pi00
	touch pkgheld/ph00
	touch pkgdep-21_1/pd21
	touch pkgdep-22_1/pd22

	cd some_repo

	flappy-create \
		-A noarch \
		-n "pkgdep-21_1" \
		-s "pkgdep" \
		../pkgdep-21_1

	atf_check_equal $? 0

	flappy-create \
		-A noarch \
		-n "pkgdep-22_1" \
		-s "pkgdep" \
		../pkgdep-22_1

	atf_check_equal $? 0

	flappy-create \
		-A noarch \
		-n "pkginst-1.0_1" \
		-s "pkginst" \
		-D "pkgdep-22_1" \
		../pkginst

	atf_check_equal $? 0

	flappy-create \
		-A noarch \
		-n "pkgheld-1.17.4_2" \
		-s "pkgheld" \
		-P "pkgdep-21_1" \
		../pkgheld

	atf_check_equal $? 0

	#ls -laR ../

	flappy-rindex -d -a pkgheld*.flappy
	atf_check_equal $? 0

	flappy-install -r root -C empty.conf --repository=$PWD -y pkgheld
	atf_check_equal $? 0
	flappy-pkgdb -r root -m hold pkgheld

	flappy-rindex -d -a pkginst*.flappy
	atf_check_equal $? 0

	flappy-rindex -d -a pkgdep-22*.flappy
	atf_check_equal $? 0

	flappy-install -r root -C empty.conf --repository=$PWD -d -yv pkginst
	atf_check_equal $? 19
}

atf_test_case hold_update_revdep

hold_update_revdep_head() {
	atf_set "descr" "Tests for pkgs on hold: update package with revdep on hold package"
}

hold_update_revdep_body() {
	mkdir -p repo empty
	cd repo
	flappy-create -A noarch -n pari-2.11.4_1 -s "pari pkg" ../empty
	flappy-create -A noarch -n pari-devel-2.11.4_1 --dependencies="pari>=2.11.4_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd pari pari-devel
	atf_check_equal $? 0

	flappy-pkgdb -r root -m hold pari
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n pari-devel-2.13.1_1 --dependencies="pari>=2.13.1_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -dvyu
	atf_check_equal $? 19
}

atf_test_case update_hold_update_revdep

update_hold_update_revdep_head() {
	atf_set "descr" "Tests for pkgs on hold: updateable held package and update package with revdep on held package"
}

update_hold_update_revdep_body() {
	mkdir -p repo empty
	cd repo
	flappy-create -A noarch -n pari-2.11.4_1 -s "pari pkg" ../empty
	flappy-create -A noarch -n pari-devel-2.11.4_1 --dependencies="pari>=2.11.4_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd pari pari-devel
	atf_check_equal $? 0

	flappy-pkgdb -r root -m hold pari
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n pari-2.13.1_1 -s "pari pkg" ../empty
	flappy-create -A noarch -n pari-devel-2.13.1_1 --dependencies="pari>=2.13.1_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -dvyu
	atf_check_equal $? 19
}

atf_test_case hold_install_revdep

hold_install_revdep_head() {
	atf_set "descr" "Tests for pkgs on hold: install package with revdep on held package"
}

hold_install_revdep_body() {
	mkdir -p repo empty
	cd repo
	flappy-create -A noarch -n pari-2.11.4_1 -s "pari pkg" ../empty
	flappy-create -A noarch -n pari-devel-2.11.4_1 --dependencies="pari>=2.11.4_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd pari
	atf_check_equal $? 0

	flappy-pkgdb -r root -m hold pari
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n pari-devel-2.13.1_1 --dependencies="pari>=2.13.1_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -dvy pari-devel
	atf_check_equal $? 19
}

atf_test_case update_hold_install_revdep

update_hold_install_revdep_head() {
	atf_set "descr" "Tests for pkgs on hold: updatable held package and install package with revdep on it"
}

update_hold_install_revdep_body() {
	mkdir -p repo empty
	cd repo
	flappy-create -A noarch -n pari-2.11.4_1 -s "pari pkg" ../empty
	flappy-create -A noarch -n pari-devel-2.11.4_1 --dependencies="pari>=2.11.4_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -yd pari
	atf_check_equal $? 0

	flappy-pkgdb -r root -m hold pari
	atf_check_equal $? 0

	cd repo
	flappy-create -A noarch -n pari-devel-2.13.1_1 --dependencies="pari>=2.13.1_1" -s "pari-devel pkg" ../empty
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/repo -dvy pari-devel
	atf_check_equal $? 19
}

atf_init_test_cases() {
	atf_add_test_case update_hold
	atf_add_test_case update_pkg_with_held_dep
	atf_add_test_case hold_update_revdep
	atf_add_test_case update_hold_update_revdep
	atf_add_test_case hold_install_revdep
	atf_add_test_case update_hold_install_revdep
}
