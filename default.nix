#!/usr/bin/env -S nix build -f

{ pkgs ? import <nixpkgs> { }
, stdenv ? pkgs.llvmPackages_latest.stdenv
}:

stdenv.mkDerivation {
	name = "riscv";
	src = ./.;
	postConfigure = ''
		makeFlagsArray+=(CPPFLAGS='-std=c99 -Wall -Wpedantic -Wno-shift-op-parentheses')
		makeFlagsArray+=(prefix="$prefix")
	'';
}
