#!/bin/sh

. ./regress.conf

dir=/tmp/gfrepattr-test.$$

pat00="g0:1, g1:1, g0:2, g2:1, g0:3, g3:1"

pat="g0:1, g1:1, g0:2, g2:1, g0:3"
g0s=g0:6
g1s=g1:1
g2s=g2:1

setup() {
    gfmkdir ${dir}
    gfrepattr -r ${dir}
}

cleanup() {
    gfrepattr -r ${dir}
    gfrmdir ${dir}
}

trap 'cleanup; exit $exit_trap' $trap_sigs

setup

gfrepattr -s -m ${dir} "${pat00}" > /dev/null 2>&1
if test $? -ne 1; then
    cleanup
    exit $exit_fail
fi

gfrepattr -s -c ${dir} "${pat00}"
if test $? -ne 0; then
    cleanup
    exit $exit_fail
fi

gfrepattr -s -m ${dir} "${pat}" > /dev/null 2>&1
if test $? -ne 0; then
    cleanup
    exit $exit_fail
fi

pat1=`gfrepattr ${dir} | \
    sed -e "s:'::g" -e "s:${dir}[ ]*\:[ ]*::" -e 's:,: :g'`
if test $? -ne 0 -o "X${pat1}" = "X"; then
    cleanup
    exit $exit_fail
fi

gotcha=0
for i in ${pat1}; do
    if test \
	"X${i}" = "X${g0s}" -o \
	"X${i}" = "X${g1s}" -o \
	"X${i}" = "X${g2s}"; then
	gotcha=`expr ${gotcha} + 1`
    fi
done

if test ${gotcha} -ne 3; then
    cleanup
    exit $exit_fail
fi

gfrepattr -r ${dir}
st=$?

cleanup

if test ${st} -eq 0; then
    exit $exit_pass
else
    exit $exit_fail
fi

