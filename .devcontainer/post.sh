#!/bin/sh
git submodule update --init --recursive
premake5 gmake2
make