{
	outputs = { self, nixpkgs }:
		let
			inherit (nixpkgs.lib) genAttrs mapAttrs systems;
			supportedSystems = systems.flakeExposed or systems.supported.hydra;
			forAllVariants = make: genAttrs supportedSystems (system: let
				pkgs = nixpkgs.legacyPackages.${system};
				stdenv = pkgs.llvmPackages_latest.stdenv;
			in rec {
				default = rv32;
				rv32 = make { inherit pkgs stdenv; xlen = 32; };
				rv64 = make { inherit pkgs stdenv; xlen = 64; };
			});
		in {
			packages = forAllVariants (import ./.);
			defaultPackage = mapAttrs (system: pkgs: pkgs.default) self.packages;
			devShells = forAllVariants (import ./shell.nix);
			devShell = mapAttrs (system: shells: shells.default) self.devShells;
		};
}
