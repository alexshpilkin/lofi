#!/usr/bin/env -S nix develop -f

{ pkgs ? import <nixpkgs> { }
, stdenv ? pkgs.stdenv
, gdb ? pkgs.gdb
, lofi ? import ./. args
, valgrind ? pkgs.valgrind
, xlen ? 32
, ...
}@args:

pkgs.mkShell.override { inherit stdenv; } {
	packages = [ gdb valgrind ];
	inputsFrom = [ lofi ];
}
