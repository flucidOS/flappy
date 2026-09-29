#!/usr/bin/env atf-sh

atf_test_case install_with_ignored_dep

install_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg install: with ignored dependency"
}

install_with_ignored_dep_body() {
	mkdir -p repo root/flappy.d pkg_A pkg_B
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/flappy.d/ignore.conf
	out=$(flappy-install -r root -C flappy.d --repository=$PWD/repo -n A)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.0_1 install noarch $PWD/repo"
	flappy-install -r root -C flappy.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	flappy-query -r root A
	atf_check_equal $? 0
	flappy-query -r root B
	atf_check_equal $? 2
}

atf_test_case update_with_ignored_dep

update_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg update: with ignored dependency"
}

update_with_ignored_dep_body() {
	mkdir -p repo root/flappy.d pkg_A pkg_B
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/flappy.d/ignore.conf
	flappy-install -r root -C flappy.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	flappy-query -r root B
	atf_check_equal $? 2
	cd repo
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	out=$(flappy-install -r root -C flappy.d --repository=$PWD/repo -un)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.1_1 update noarch $PWD/repo"
	flappy-install -r root -C flappy.d --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(flappy-query -r root -p pkgver A)
	atf_check_equal $out A-1.1_1
	flappy-query -r root B
	atf_check_equal $? 2
}

atf_test_case remove_with_ignored_dep

remove_with_ignored_dep_head() {
	atf_set "descr" "Tests for pkg remove: with ignored dependency"
}

remove_with_ignored_dep_body() {
	mkdir -p repo root/flappy.d pkg_A pkg_B
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	echo "ignorepkg=B" > root/flappy.d/ignore.conf
	flappy-install -r root -C flappy.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	flappy-query -r root B
	atf_check_equal $? 2
	out=$(flappy-remove -r root -C flappy.d -Rn A)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "A-1.0_1 remove noarch $PWD/repo"
	flappy-remove -r root -C flappy.d -Ryvd A
	atf_check_equal $? 0
	flappy-query -r root A
	atf_check_equal $? 2
	flappy-query -r root B
	atf_check_equal $? 2
}

atf_test_case remove_ignored_dep

remove_ignored_dep_head() {
	atf_set "descr" "Tests for pkg remove: pkg is dependency but ignored"
}

remove_ignored_dep_body() {
	mkdir -p repo root/flappy.d pkg_A pkg_B
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" -D "B-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C flappy.d --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	echo "ignorepkg=B" > root/flappy.d/ignore.conf
	out=$(flappy-remove -r root -C flappy.d -Rn B)
	set -- $out
	exp="$1 $2 $3 $4"
	atf_check_equal "$exp" "B-1.0_1 remove noarch $PWD/repo"
	flappy-remove -r root -C flappy.d -Ryvd B
	atf_check_equal $? 0
	flappy-query -r root A
	atf_check_equal $? 0
	flappy-query -r root B
	atf_check_equal $? 2
}

atf_init_test_cases() {
	atf_add_test_case install_with_ignored_dep
	atf_add_test_case update_with_ignored_dep
	atf_add_test_case remove_with_ignored_dep
	atf_add_test_case remove_ignored_dep
}
