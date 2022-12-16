{
	outputs = { self, nixpkgs }:
		let
			inherit (nixpkgs.lib) genAttrs mapAttrs systems;
			supportedSystems = systems.flakeExposed or systems.supported.hydra;
		in {
			packages = genAttrs supportedSystems (system:
				let
					pkgs = nixpkgs.legacyPackages.${system};
					makePackage = args: import ./. ({ inherit pkgs; } // args);
				in rec {
					default = rv32;
					rv32 = makePackage { xlen = 32; };
					rv64 = makePackage { xlen = 64; };
				});
			defaultPackage = mapAttrs (system: pkgs: pkgs.default) self.packages;
			devShells = genAttrs supportedSystems (system: {
				default = import ./shell.nix {
					pkgs = nixpkgs.legacyPackages.${system};
				};
			});
			devShell = mapAttrs (system: shells: shells.default) self.devShells;
		};
}
