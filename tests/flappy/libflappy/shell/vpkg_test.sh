#!/usr/bin/env atf-sh
#

# This test case is a bit special because it stresses how virtual packages
# are handled in flappy.
#
# - A-1.0 is installed, provides vpkg libEGL-1.0.
# - B-1.0 is installed and depends on A.
# - C-1.0 is installed and depends on libEGL>=2.0.
# - D-1.0 is installed as dependency of C, and provides libEGL-2.0.
# - A should not be updated to D.
#
# D should replace A only if it has "replaces" property on A. The result should be
# that D must be installed and A being as is.

atf_test_case vpkg_dont_update

vpkg_dont_update_head() {
	atf_set "descr" "Tests for virtual pkgs: don't update incompatible vpkg provider"
}

vpkg_dont_update_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libEGL-1.0_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "libEGL>=2.0" ../pkg_C
	atf_check_equal $? 0
	flappy-create -A noarch -n D-1.0_1 -s "D pkg" --provides "libEGL-2.0_1" ../pkg_D
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	flappy-install -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 0

	out=$(flappy-query  -r root -l|awk '{print $2}'|tr -d '\n')
	exp="A-1.0_1B-1.0_1C-1.0_1D-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg_replace_provider

vpkg_replace_provider_head() {
	atf_set "descr" "Tests for virtual pkgs: replace provider in update (commit ebc0f27ae1c)"
}

vpkg_replace_provider_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0
	flappy-create -A noarch -n C-1.0_1 -s "C pkg" --provides "A-1.0_1" --replaces="A>=0" ../pkg_C
	atf_check_equal $? 0
	flappy-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0" ../pkg_D
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install  -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	flappy-install  -r root --repository=$PWD/some_repo -dy D
	atf_check_equal $? 0

	out=$(flappy-query  -r root -l|awk '{print $2}'|tr -d '\n')
	exp="B-1.0_1C-1.0_1D-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg_provider_in_trans

vpkg_provider_in_trans_head() {
	atf_set "descr" "Tests for virtual pkgs: provider in transaction"
}

vpkg_provider_in_trans_body() {
	mkdir some_repo
	mkdir -p pkg_gawk pkg_base-system pkg_busybox
	cd some_repo
	flappy-create -A noarch -n gawk-1.0_1 -s "gawk pkg" ../pkg_gawk
	atf_check_equal $? 0
	flappy-create -A noarch -n base-system-1.0_1 -s "base-system pkg" --dependencies "gawk>=0" ../pkg_base-system
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install  -r root --repository=$PWD/some_repo -dy base-system
	atf_check_equal $? 0

	cd some_repo

	flappy-create -A noarch -n gawk-1.1_1 -s "gawk pkg" --provides "awk-1.0_1" --replaces "awk>=0" ../pkg_gawk
	atf_check_equal $? 0

	flappy-create -A noarch -n busybox-1.0_1 -s "busybox awk" --provides "awk-1.0_1" --replaces "awk>=0" ../pkg_busybox
	atf_check_equal $? 0

	flappy-create -A noarch -n base-system-1.1_1 -s "base-system pkg" --dependencies "awk>=0" ../pkg_base-system
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install  -r root --repository=$PWD/some_repo -dyu
	atf_check_equal $? 0

	out=$(flappy-query  -r root -l|awk '{print $2}'|tr -d '\n')
	exp="base-system-1.1_1gawk-1.1_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg_provider_in_conf

vpkg_provider_in_conf_head() {
	atf_set "descr" "Tests for virtual pkgs: provider in configuration file"
}

vpkg_provider_in_conf_body() {
	mkdir some_repo
	mkdir -p pkg_gawk
	cd some_repo
	flappy-create -A noarch -n gawk-1.0_1 -s "gawk pkg" ../pkg_gawk
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	cd ..
	flappy-install  -r root --repository=$PWD/some_repo -dy gawk
	atf_check_equal $? 0

	mkdir -p root/flappy.d

	echo "virtualpkg=awk:gawk" > root/flappy.d/awk.conf
	out=$(flappy-query -C flappy.d -r root --repository=$PWD/some_repo -ppkgver awk)
	exp="gawk-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp

	echo "virtualpkg=awk-0_1:gawk" > root/flappy.d/awk.conf
	out=$(flappy-query -C flappy.d -r root --repository=$PWD/some_repo -ppkgver awk)
	exp="gawk-1.0_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg_dep_provider_in_conf

vpkg_dep_provider_in_conf_head() {
	atf_set "descr" "Tests for virtual pkgs: dependency provider in configuration file"
}

vpkg_dep_provider_in_conf_body() {
	mkdir some_repo
	mkdir -p pkg_gawk pkg_blah
	cd some_repo
	flappy-create -A noarch -n blah-1.0_1 -s "blah pkg" ../pkg_blah
	atf_check_equal $? 0
	flappy-create -A noarch -n gawk-1.0_1 -s "gawk pkg" --dependencies "vpkg>=4.4" ../pkg_gawk
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	mkdir -p root/flappy.d
	echo "virtualpkg=vpkg:blah" > root/flappy.d/blah.conf
	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -dy gawk
	atf_check_equal $? 0

	rm -rf root
	mkdir -p root/flappy.d
	echo "virtualpkg=vpkg-4.4_1:blah" > root/flappy.d/blah.conf
	flappy-install -C flappy.d -r root --repository=$PWD/some_repo -dy gawk
	atf_check_equal $? 0

}

atf_test_case vpkg_incompat_upgrade

vpkg_incompat_upgrade_head() {
	atf_set "descr" "Tests for virtual pkgs: incompat provider upgrade"
}

vpkg_incompat_upgrade_body() {
	mkdir some_repo
	mkdir -p pkg_A pkg_B
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "vpkg-9_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "vpkg-9_1" ../pkg_B
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0

	cd some_repo
	flappy-create -A noarch -n A-2.0_1 -s "A pkg" --provides "vpkg-10_1" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dyuv
	# ENODEV == unresolved dependencies
	atf_check_equal $? 19
}

atf_test_case vpkg_incompat_downgrade

vpkg_incompat_downgrade_head() {
	atf_set "descr" "Tests for virtual pkgs: incompat provider downgrade"
}

vpkg_incompat_downgrade_body() {
	mkdir some_repo
	mkdir -p pkg_A pkg_B
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "vpkg-9_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "vpkg-9_1" ../pkg_B
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0

	cd some_repo
	flappy-create -A noarch -n A-0.1_1 -s "A pkg" --provides "vpkg-8_1" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -f -a $PWD/A-0.1_1.*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dyvf A-0.1_1
	# ENODEV == unresolved dependencies
	atf_check_equal $? 19
}

atf_test_case vpkg_provider_and_revdeps_downgrade

vpkg_provider_and_revdeps_downgrade_head() {
	atf_set "descr" "Tests for virtual pkgs: downgrade vpkg provider and its revdeps"
}

vpkg_provider_and_revdeps_downgrade_body() {
	mkdir some_repo
	mkdir -p pkg_A pkg_B
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "vpkg-9_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "vpkg-9_1" ../pkg_B
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0

	cd some_repo
	flappy-create -A noarch -n A-0.1_1 -s "A pkg" --provides "vpkg-8_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-0.1_1 -s "B pkg" --dependencies "vpkg-8_1" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -f -a $PWD/A-0.1_1.*.flappy $PWD/B-0.1_1.*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dyvf B-0.1_1 A-0.1_1
	atf_check_equal $? 0

	out=$(flappy-query  -r root -l|awk '{print $2}'|tr -d '\n')
	exp="A-0.1_1B-0.1_1"
	echo "out: $out"
	echo "exp: $exp"
	atf_check_equal $out $exp
}

atf_test_case vpkg_provider_remove

vpkg_provider_remove_head() {
	atf_set "descr" "Tests for virtual pkgs: removing a vpkg provider"
}

vpkg_provider_remove_body() {
	mkdir some_repo
	mkdir -p pkg_A pkg_B
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" --provides "vpkg-9_1" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "vpkg-9_1" ../pkg_B
	atf_check_equal $? 0

	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0

	flappy-remove -r root -dyv A
	# ENODEV == unresolved dependencies
	atf_check_equal $? 19
}


atf_test_case vpkg_multirepo

vpkg_multirepo_head() {
       atf_set "descr" "Tests for virtual pkgs: vpkg provider in multiple repos"
}

vpkg_multirepo_body() {
       mkdir empty repo-1 repo-2

       cd repo-1
       flappy-create -n A-1.0_1 -s A -A noarch -P V-0_1 ../empty
       atf_check_equal $? 0

       cd ../repo-2
       flappy-create -n B-1.0_1 -s B -A noarch -P V-0_1 ../empty
       atf_check_equal $? 0

       cd ..
       flappy-rindex -a repo-1/*.flappy
       atf_check_equal $? 0
       flappy-rindex -a repo-2/*.flappy
       atf_check_equal $? 0

       echo "virtualpkg=V-0_1:B" > virtualpkg.conf
       out="$(flappy-install -C $PWD -r root --repo=repo-1 --repo=repo-2 -n V|awk '{print $1}')"
       atf_check_equal "$out" "B-1.0_1"
}

atf_init_test_cases() {
	atf_add_test_case vpkg_dont_update
	atf_add_test_case vpkg_replace_provider
	atf_add_test_case vpkg_provider_in_trans
	atf_add_test_case vpkg_provider_in_conf
	atf_add_test_case vpkg_dep_provider_in_conf
	atf_add_test_case vpkg_incompat_upgrade
	atf_add_test_case vpkg_incompat_downgrade
	atf_add_test_case vpkg_provider_and_revdeps_downgrade
	atf_add_test_case vpkg_provider_remove
	atf_add_test_case vpkg_multirepo
}
