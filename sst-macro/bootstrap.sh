#!/bin/sh

git submodule init
git submodule update
ltool=`which glibtoolize`
if [ "$?" -ne "0" ]; then
  ltool="libtoolize"
fi
$ltool \
  && (cd sst-dumpi && $ltool ) \
  && autoreconf --force --install

