#!/usr/bin/env -S nix build -f

{ pkgs ? import <nixpkgs> { }
, stdenv ? pkgs.llvmPackages_latest.stdenv
, xlen ? 32
}:

stdenv.mkDerivation {
	name = "riscv";
	src = ./.;
	postConfigure = ''
		makeFlagsArray+=(CPPFLAGS='-DXWORD_BIT=${toString xlen} -std=c99 -Wall -Wpedantic -Wno-bitwise-op-parentheses -Wno-shift-op-parentheses')
		makeFlagsArray+=(prefix="$prefix")
	'';
}
