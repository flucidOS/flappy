#! /usr/bin/env atf-sh

atf_test_case install_existent

install_existent_head() {
	atf_set "descr" "flappy-install(1): install multiple existent pkgs (issue #53)"
}

install_existent_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00
	touch pkg_B/file00
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A B
	atf_check_equal $? 0

	rm -r root
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y B A
	atf_check_equal $? 0
}

atf_test_case update_existent

updated_existent_head() {
	atf_set "descr" "flappy-install(1): update existent pkg"
}

update_existent_body() {
	mkdir -p some_repo pkg_A pkg_B
	touch pkg_A/file00
	touch pkg_B/file00
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.1_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -un A B
	atf_check_equal $? 0
}

atf_test_case update_unpacked

update_unpacked_head() {
	atf_set "descr" "flappy-install(1): update unpacked pkg"
}

update_unpacked_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check_equal $? 0
	cd some_repo
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	set -- $(flappy-install -r root -C empty.conf --repository=$PWD/some_repo -un A)
	if [ "$2" != "update" ]; then
		atf_fail "'$2' != 'update'"
	fi
}

atf_test_case unpacked_dep

unpacked_dep_head() {
	atf_set "descr" "flappy-install(1): unpacked dependency"
}

unpacked_dep_body() {
	mkdir -p some_repo pkg_A pkg_B

	cd some_repo
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -- flappy-create -A noarch -n B-1.0_1 -s "B pkg" -D "A>=0" ../pkg_B
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..

	atf_check -o ignore -- flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check -o inline:"unpacked\n" -- flappy-query -r root -p state A
	atf_check -o match:"A-1.0_1 configure" -o match:"B-1.0_1 install" -- \
		flappy-install -r root -C empty.conf --repository=$PWD/some_repo -un B
}

atf_test_case unpacked_dep_missing

unpacked_dep_missing_head() {
	atf_set "descr" "flappy-install(1): unpacked dependency (missing)"
}

unpacked_dep_missing_body() {
	mkdir -p some_repo other_repo pkg_A pkg_B

	cd some_repo
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ../other_repo
	atf_check -o ignore -- flappy-create -A noarch -n B-1.0_1 -s "B pkg" -D "A>=0" ../pkg_B
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..

	atf_check -o ignore -- flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check -o inline:"unpacked\n" -- flappy-query -r root -p state A
	atf_check -o match:"A-1.0_1 configure" -o match:"B-1.0_1 install" -- \
		flappy-install -r root -C empty.conf --repository=$PWD/other_repo -un B
}

atf_test_case reinstall_unpacked_unpack_only

reinstall_unpacked_unpack_only_head() {
	atf_set "descr" "flappy-install(1): reinstall unpacked packages with unpack-only"
}

reinstall_unpacked_unpack_only_body() {
	mkdir -p some_repo pkg_A
	touch pkg_A/file00
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yU A
	atf_check_equal $? 0
	set -- $(flappy-install -r root -C empty.conf --repository=$PWD/some_repo -fUn A)
	if [ "$2" != "reinstall" ]; then
		atf_fail "'$2' != 'reinstall'"
	fi
}

atf_test_case reproducible

reproducible_head() {
	atf_set "descr" "flappy-install(1): test --reproducible"
}

reproducible_body() {
	mkdir -p repo-1 repo-2 pkg_A
	touch pkg_A/file
	cd repo-1
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ../repo-2
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root-1 --repo=$PWD/repo-1 --repro -y A
	atf_check_equal $? 0
	flappy-install -r root-2 --repo=$PWD/repo-2 --repro -y A
	atf_check_equal $? 0

	# Compare pkgdb in both rootdirs
	cmp -s root-1/var/db/flappy/pkgdb-0.38.plist root-2/var/db/flappy/pkgdb-0.38.plist
	atf_check_equal $? 0

	# Now check without --reproducible
	rm -rf root-1 root-2

	flappy-install -r root-1 --repo=$PWD/repo-1 --repro -y A
	atf_check_equal $? 0
	flappy-install -r root-2 --repo=$PWD/repo-2 -y A
	atf_check_equal $? 0

	# Compare pkgdb in both rootdirs
	cmp -s root-1/var/db/flappy/pkgdb-0.38.plist root-2/var/db/flappy/pkgdb-0.38.plist
	atf_check_equal $? 1
}

atf_test_case install_msg

install_msg_head() {
	atf_set "descr" "flappy-install(1): show install message"
}

install_msg_body() {
	mkdir -p some_repo pkg_A

	# install will now show the message
	cat <<-EOF >pkg_A/INSTALL.msg
	foobar-install-msg
	EOF
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	atf_check -s exit:0 \
		-o 'match:foobar-install-msg' \
		-e ignore \
		-- flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A

	# update with the same message will not show the message
	cd some_repo
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	atf_check -s exit:0 \
		-o 'not-match:foobar-install-msg' \
		-e ignore \
		-- flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yu

	# update with new message will show the message
	cat <<-EOF >pkg_A/INSTALL.msg
	fizzbuzz-install-msg
	EOF
	cd some_repo
	flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	atf_check -s exit:0 \
		-o 'match:fizzbuzz-install-msg' \
		-e ignore \
		-- flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yu
}

atf_test_case already_installed

already_installed_head() {
       atf_set "descr" "flappy-install(1): various ways to install already installed packages"
}

already_installed_body() {
       mkdir -p repo pkg

       cd repo
       atf_check -o ignore -- flappy-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg
       atf_check -e ignore -o ignore -- flappy-rindex -a *.flappy
       cd ..

       atf_check -o ignore -e ignore -- flappy-install -r root -R repo -y A
       atf_check -o ignore -e match:"ERROR: Package \`A' already installed." -- flappy-install -r root -R repo -y A
       atf_check -o ignore -e match:"ERROR: Package \`A-1.1_1' already installed." -- flappy-install -r root -R repo -y A-1.1_1
       atf_check -s exit:2 -o ignore -e match:"ERROR: Package 'A-1.2_1' not found in repository pool." -- flappy-install -r root -R repo -y A-1.2_1

       cd repo
       atf_check -o ignore -- flappy-create -A noarch -n A-1.2_1 -s "A pkg" ../pkg
       atf_check -e ignore -o ignore -- flappy-rindex -a *.flappy
       cd ..

       atf_check -o ignore -e ignore -- flappy-install -r root -R repo -y A
       atf_check -o ignore -e match:"ERROR: Package \`A' already installed." -- flappy-install -r root -R repo -y A
       atf_check -s exit:2 -o ignore -e match:"ERROR: Package 'A-1.1_1' not found in repository pool." -- flappy-install -r root -R repo -y A-1.1_1
       atf_check -o ignore -e match:"ERROR: Package \`A-1.2_1' already installed." -- flappy-install -r root -R repo -y A-1.2_1
}

atf_init_test_cases() {
	atf_add_test_case install_existent
	atf_add_test_case update_existent
	atf_add_test_case update_unpacked
	atf_add_test_case unpacked_dep
	atf_add_test_case unpacked_dep_missing
	atf_add_test_case reinstall_unpacked_unpack_only
	atf_add_test_case reproducible
	atf_add_test_case install_msg
	atf_add_test_case already_installed
}
