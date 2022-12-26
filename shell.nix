#!/usr/bin/env -S nix develop -f

{ pkgs ? import <nixpkgs> { }
, gdb ? pkgs.gdb
, stdenv ? pkgs.stdenv
, valgrind ? pkgs.valgrind
}:

pkgs.mkShell.override { inherit stdenv; } {
	packages = [ gdb valgrind ];
}
