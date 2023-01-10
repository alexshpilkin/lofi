#!/usr/bin/env -S nix build -f

{ pkgs ? import <nixpkgs> { }
, stdenv ? pkgs.stdenv
, dtc ? pkgs.dtc
, riscv-tests ? import ./riscv-tests.nix args
, spike ? pkgs.spike
, doCheck ? true
, xlen ? 32
, ...
}@args:

stdenv.mkDerivation {
	name = "lofi";
	src = ./.;

	checkInputs = [ dtc spike ];

	CPPFLAGS = "-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wpedantic -Wno-parentheses";
	makeFlags = [
		"TESTS=${riscv-tests}/share/riscv-tests"
		"XLEN=${toString xlen}"
	];
	hardeningDisable = [ "all" ];
	postConfigure = ''
		makeFlagsArray+=(prefix="$prefix")
	'';
	inherit doCheck;
	checkTarget = "check";
}
