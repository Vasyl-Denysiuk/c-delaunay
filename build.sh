#!/bin/zsh
gcc ray.c delaunay.c $(pkg-config --libs --cflags raylib)
