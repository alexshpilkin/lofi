#!/usr/bin/env -S nix develop -f

{ pkgs ? import <nixpkgs> { }
, gdb ? pkgs.gdb
, lofi ? import ./. args
, valgrind ? pkgs.valgrind
, xlen ? 32
, ...
}@args:

pkgs.mkShell {
	packages = [ gdb valgrind ];
	inputsFrom = [ lofi ];
}
