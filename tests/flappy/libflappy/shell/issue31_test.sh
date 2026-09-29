#! /usr/bin/env atf-sh

# flappy issue #31.
# How to reproduce it:
# 	Generate pkg A-0.1_1:
#		/dir/dir/foo
#	Install pkg A.
#	Generate pkg A-0.2_1:
#		/dir/foo
#	Update pkg A to 0.2_1

atf_test_case issue31

issue31_head() {
	atf_set "descr" "flappy issue #31 (https://github.com/xtraeme/flappy/issues/31)"
}

issue31_body() {
	mkdir -p pkg_A/usr/share/licenses/chromium/license.html
	echo random > pkg_A/usr/share/licenses/chromium/license.html/eula.html
	flappy-create -A noarch -n A-0.1_1 -s "pkg A" pkg_A
	atf_check_equal $? 0
	flappy-rindex -d -a $PWD/*.flappy
	atf_check_equal $? 0

	flappy-install -r root -C null.conf --repository=$PWD -y A
	atf_check_equal $? 0

	touch root/usr/share/licenses/chromium/license.html/fail

	mkdir -p pkg_B/usr/share/licenses/chromium
	echo "morerandom" > pkg_B/usr/share/licenses/chromium/license.html
	flappy-create -A noarch -n A-0.2_1 -s "pkg A" pkg_B
	atf_check_equal $? 0
	flappy-rindex -d -a A-0.2_1.noarch.flappy
	atf_check_equal $? 0

	flappy-install -r root -C null.conf --repository=$PWD -yuvd A
	# ENOTEMPTY
	atf_check_equal $? 39

	rm root/usr/share/licenses/chromium/license.html/fail
	flappy-install -r root -C null.conf --repository=$PWD -yuvd A
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case issue31
}
