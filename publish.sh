#!/bin/bash
# Launchpad publishing script
# Copyright 2012 - Pavel Kalian (pavel@kalian.cz)
# Licensed under the terms of GPLv2+

# Use it:
#    a.  $cd ~/Projects/ofc_pi
#    b.  edit publish.sh, change the version number
#    c.  $rm -rf /tmp/launchpad
#    e.  $./publish.sh "Beta Version 0.1.412"
#        note that the script may use the [beta-ppa] or [ocpn-ppa]

VERSION=0.1.412
AUTHOR='Dave Register <bdbcat@yahoo.com>'
DATE=`date -R`
SERIES=1
Ubuntus=('xenial' 'trusty' 'precise' 'bionic')

WORKDIR=/tmp/launchpad
BRANCH=master

MYDIR=`pwd`
if [ $# -lt 1 ] ; then
 echo You must supply changelog message
 exit 0
fi

mkdir $WORKDIR
git archive $BRANCH | tar -x -C $WORKDIR

rm -rf $WORKDIR/wxWidgets
rm -rf $WORKDIR/buildosx
rm -rf $WORKDIR/buildwin
rm -rf $WORKDIR/buildandroid
rm -rf $WORKDIR/build_debug
rm -rf $WORKDIR/build_no_local
rm -rf $WORKDIR/include/GL/

TOMOVE=`ls -d $WORKDIR/*`

mkdir $WORKDIR/ofc_pi
mv $TOMOVE $WORKDIR/ofc_pi

tar -cf $WORKDIR/ofc_pi_$VERSION.orig.tar -C $WORKDIR/ofc_pi .
cp $WORKDIR/ofc_pi_$VERSION.orig.tar $WORKDIR/ofc-pi_$VERSION.orig.tar
xz $WORKDIR/ofc-pi_$VERSION.orig.tar

cp -rf debian $WORKDIR/ofc_pi

read -p "Press [Enter] to publish (now it's time to apply patches manually if needed)"

for u in "${Ubuntus[@]}"
do
 cat changelog.tpl|sed "s/VERSION/$VERSION/g"|sed "s/UBUNTU/$u/g"|sed "s/SERIES/$SERIES/g"|sed "s/MESSAGE/$1/g"|sed "s/AUTHOR/$AUTHOR/g"|sed "s/TIMESTAMP/$DATE/g" > $WORKDIR/dummy
 cat $WORKDIR/ofc_pi/debian/changelog >> $WORKDIR/dummy
 cp $WORKDIR/dummy $WORKDIR/ofc_pi/debian/changelog
 cd $WORKDIR/ofc_pi

 #  -sa option forces the inclusion of the full source package as well as the changes
 #debuild  -sa -k0x2E50AC4A -S
 debuild  -k0x2E50AC4A -S

 #dput -f ocpn-ppa ../ofc-pi_$VERSION-0~"$u""$SERIES"_source.changes
 dput -f beta-ppa ../ofc-pi_$VERSION-0~"$u""$SERIES"_source.changes
 cd $MYDIR
done

cp $WORKDIR/ofc_pi/debian/changelog debian
rm -rf $WORKDIR
