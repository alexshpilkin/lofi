{
	outputs = { self, nixpkgs }:
		let
			inherit (nixpkgs.lib) genAttrs mapAttrs systems;
			supportedSystems = systems.flakeExposed or systems.supported.hydra;
		in {
			packages = genAttrs supportedSystems (system: {
				default = import ./. {
					pkgs = nixpkgs.legacyPackages.${system};
				};
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
