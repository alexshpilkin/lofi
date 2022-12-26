#!/usr/bin/env -S nix build -f

{ pkgs ? import <nixpkgs> { }
, stdenv ? pkgs.stdenv
, xlen ? 32
}:

stdenv.mkDerivation {
	name = "riscv";
	src = ./.;

	CPPFLAGS = "-DXWORD_BIT=${toString xlen} -std=c99 -Wall -Wpedantic -Wno-parentheses";
	postConfigure = ''
		makeFlagsArray+=(prefix="$prefix")
	'';
}
