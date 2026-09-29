#! /usr/bin/env atf-sh

atf_test_case remove_directory

remoe_directory_head() {
	atf_set "descr" "flappy-remove(1): remove nested directories"
}

remove_directory_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/B/C/file00
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	flappy-remove -r root -C empty.conf -y A
	atf_check_equal $? 0
	test -d root/B
	atf_check_equal $? 1
}

atf_test_case remove_orphans

remove_orphans_head() {
	atf_set "descr" "flappy-remove(1): remove orphaned packages"
}

remove_orphans_body() {
	mkdir -p some_repo pkg_A/B/C
	touch pkg_A/
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	flappy-install -r root -C empty.conf --repository=$PWD/some_repo -yA A
	atf_check_equal $? 0
	flappy-remove -r root -C empty.conf -yvdo
	atf_check_equal $? 0
	flappy-query -r root A
	atf_check_equal $? 2
}

atf_test_case clean_cache

clean_cache_head() {
	atf_set "descr" "flappy-remove(1): clean cache"
}

clean_cache_body() {
	mkdir -p repo pkg_A/B/C pkg_B
	touch pkg_A/

	cd repo
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check -o ignore -- flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..

	mkdir -p root/etc/flappy.d root/var/db/flappy/https___localhost_ root/var/cache/flappy
	atf_check -- cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check -- cp repo/*.flappy root/var/cache/flappy
	echo "repository=https://localhost/" >root/etc/flappy.d/localrepo.conf

	atf_check -o ignore -- flappy-install -r root -C etc/flappy.d -R repo -y B
	atf_check -o inline:"Removed A-1.0_1.noarch.flappy from cachedir (obsolete)\n" \
		-- flappy-remove -r root -C etc/flappy.d -O
	atf_check -- test -f root/var/cache/flappy/A-1.0_2.noarch.flappy
	atf_check -s exit:1 -- test -f root/var/cache/flappy/A-1.0_1.noarch.flappy
	atf_check -- test -f root/var/cache/flappy/B-1.0_1.noarch.flappy
}

atf_test_case clean_cache_dry_run

clean_cache_dry_run_head() {
	atf_set "descr" "flappy-remove(1): clean cache dry run"
}

clean_cache_dry_run_body() {
	mkdir -p repo pkg_A/B/C
	touch pkg_A/
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/flappy.d root/var/db/flappy/https___localhost_ root/var/cache/flappy
	cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check_equal $? 0
	cp repo/*.flappy root/var/cache/flappy
	atf_check_equal $? 0
	echo "repository=https://localhost/" >root/etc/flappy.d/localrepo.conf
	ls -lsa root/var/cache/flappy
	out="$(flappy-remove -r root -C etc/flappy.d -dvnO)"
	atf_check_equal $? 0
	atf_check_equal "$out" "Removed A-1.0_1.noarch.flappy from cachedir (obsolete)"
}

atf_test_case clean_cache_dry_run_perm

clean_cache_dry_run_perm_head() {
	atf_set "descr" "flappy-remove(1): clean cache dry run without read permissions"
}

clean_cache_dry_run_perm_body() {
	# this should print an error instead of dry deleting the files it can't read
	mkdir -p repo pkg_A/B/C
	touch pkg_A/
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/flappy.d root/var/db/flappy/https___localhost_ root/var/cache/flappy
	cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check_equal $? 0
	cp repo/*.flappy root/var/cache/flappy
	atf_check_equal $? 0
	chmod 0000 root/var/cache/flappy/*.flappy
	echo "repository=https://localhost/" >root/etc/flappy.d/localrepo.conf
	out="$(flappy-remove -r root -C etc/flappy.d -dvnO)"
	atf_check_equal $? 0
	atf_check_equal "$out" "Removed A-1.0_1.noarch.flappy from cachedir (obsolete)"
}

clean_cache_uninstalled_head() {
	atf_set "descr" "flappy-remove(1): clean uninstalled package from cache"
}

clean_cache_uninstalled_body() {
	mkdir -p repo pkg_A/B/C pkg_B
	touch pkg_A/
	cd repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-create -A noarch -n B-1.0_1 -s "B pkg" ../pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..
	mkdir -p root/etc/flappy.d root/var/db/flappy/https___localhost_ root/var/cache/flappy
	cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check_equal $? 0
	cp repo/*.flappy root/var/cache/flappy
	atf_check_equal $? 0
	echo "repository=https://localhost/" >root/etc/flappy.d/localrepo.conf
	flappy-install -r root -C etc/flappy.d -R repo -dvy B
	atf_check_equal $? 0
	flappy-remove -r root -C etc/flappy.d -dvOO
	atf_check_equal $? 0
	test -f root/var/cache/flappy/A-1.0_2.noarch.flappy
	atf_check_equal $? 1
	test -f root/var/cache/flappy/A-1.0_1.noarch.flappy
	atf_check_equal $? 1
	test -f root/var/cache/flappy/B-1.0_1.noarch.flappy
	atf_check_equal $? 0
}

clean_cache_installed_head() {
	atf_set "descr" "flappy-remove(1): do not clean currently installed packages"
}

clean_cache_installed_body() {
	mkdir -p repo pkg_A/B/C pkg_B
	mkdir -p root/etc/flappy.d root/var/db/flappy/https___localhost_ root/var/cache/flappy

	cd repo
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check -o ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..
	atf_check -- cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check -- cp repo/*.flappy root/var/cache/flappy

	echo "repository=https://localhost/" >root/etc/flappy.d/localrepo.conf
	atf_check -o ignore -e ignore -- flappy-install -r root -C etc/flappy.d -R repo -dvy A

	cd repo
	atf_check -o ignore -- flappy-create -A noarch -n A-1.0_2 -s "A pkg" ../pkg_A
	atf_check -o ignore -e ignore -- flappy-rindex -a $PWD/*.flappy
	cd ..
	atf_check -- cp repo/*-repodata root/var/db/flappy/https___localhost_
	atf_check -- cp repo/*.flappy root/var/cache/flappy

	atf_check -- flappy-remove -r root -C etc/flappy.d -O
	atf_check -- test -f root/var/cache/flappy/A-1.0_2.noarch.flappy
	atf_check -- test -f root/var/cache/flappy/A-1.0_1.noarch.flappy
}

atf_test_case remove_msg

remove_msg_head() {
	atf_set "descr" "flappy-rmeove(1): show remove message"
}

remove_msg_body() {
	mkdir -p some_repo pkg_A

	cat <<-EOF >pkg_A/REMOVE.msg
	foobar-remove-msg
	EOF
	cd some_repo
	flappy-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0
	cd ..

	flappy-install -r root -C empty.conf -R some_repo -dvy A
	atf_check_equal $? 0

	atf_check -s exit:0 \
		-o 'match:foobar-remove-msg' \
		-e ignore \
		-- flappy-remove -r root -C empty.conf -y A
}

atf_init_test_cases() {
	atf_add_test_case remove_directory
	atf_add_test_case remove_orphans
	atf_add_test_case clean_cache
	atf_add_test_case clean_cache_dry_run
	atf_add_test_case clean_cache_dry_run_perm
	atf_add_test_case clean_cache_uninstalled
	atf_add_test_case clean_cache_installed
	atf_add_test_case remove_msg
}
