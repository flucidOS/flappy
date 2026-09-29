#! /usr/bin/env atf-sh
# Test that flappy-uhelper arch works as expected.

atf_test_case native

native_head() {
	atf_set "descr" "flappy-uhelper arch: native test"
}

native_body() {
	unset FLAPPY_ARCH FLAPPY_TARGET_ARCH
	atf_check -o "inline:$(uname -m)\n" -- flappy-uhelper -r "$PWD" arch
}

atf_test_case env

env_head() {
	atf_set "descr" "flappy-uhelper arch: envvar override test"
}
env_body() {
	export FLAPPY_ARCH=foo
	atf_check_equal $(flappy-uhelper -r $PWD arch) foo
}

atf_test_case conf

conf_head() {
	atf_set "descr" "flappy-uhelper arch: configuration override test"
}
conf_body() {
	mkdir -p flappy.d root
	unset FLAPPY_ARCH FLAPPY_TARGET_ARCH
	echo "architecture=foo" > flappy.d/arch.conf
	atf_check -o inline:"foo\n" -- flappy-uhelper -C $PWD/flappy.d -r root arch
}

atf_init_test_cases() {
	atf_add_test_case native
	atf_add_test_case env
	atf_add_test_case conf
}
