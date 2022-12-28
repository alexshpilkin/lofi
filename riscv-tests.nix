#!/usr/bin/env -S nix build -f

{ pkgs ? import <nixpkgs> { }
, hostPkgs ? pkgs.pkgsCross."riscv${toString xlen}-embedded"
, xlen ? 32
, ...
}:

hostPkgs.stdenv.mkDerivation {
	name = "riscv${toString xlen}-tests";
	src = ./riscv-tests;

	configureFlags = [
		"--with-xlen=${toString xlen}"
		"--target=riscv${toString xlen}-none-elf"
	];
	hardeningDisable = [ "all" ];
}
