#!/usr/bin/env -S nix develop -f

{ pkgs ? import <nixpkgs> { }
, gdb ? pkgs.gdb
, stdenv ? pkgs.llvmPackages_latest.stdenv
, valgrind ? pkgs.valgrind
}:

pkgs.mkShell.override { inherit stdenv; } {
	packages = [ gdb valgrind ];
}
