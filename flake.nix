{
	outputs = { self, nixpkgs }:
		let
			inherit (nixpkgs.lib) genAttrs mapAttrs systems;
			supportedSystems = systems.flakeExposed or systems.supported.hydra;
		in {
			devShells = genAttrs supportedSystems (system: {
				default = import ./shell.nix {
					pkgs = nixpkgs.legacyPackages.${system};
				};
			});
			devShell = mapAttrs (system: shells: shells.default) self.devShells;
		};
}
