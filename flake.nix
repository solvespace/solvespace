{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    systems.url = "github:nix-systems/x86_64-linux";

    flake-utils.url = "github:numtide/flake-utils";
    flake-utils.inputs.systems.follows = "systems";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      ...
    }:

    (
      let
        rev = self.sourceInfo.rev or self.sourceInfo.dirtyRev;
      in
      {
        overlays.solvespace = self: super: {
          solvespace-latest = super.callPackage ./nix-support/package.nix {
            inherit rev;
          };
        };
        overlays.default = self.overlays.solvespace;
      }
    )
    // (flake-utils.lib.eachDefaultSystem (
      system:
      let
        inherit
          (import nixpkgs {
            inherit system;
            overlays = [ self.overlays.solvespace ];
          })
          nixfmt-rfc-style
          solvespace
          callPackage
          ;
      in
      {
        formatter = nixfmt-rfc-style;

        packages.solvespace = solvespace;
        packages.default = self.packages.${system}.solvespace;

        devShells.solvespace = callPackage ./nix-support/shell.nix { };
        devShells.default = self.devShells.${system}.solvespace;
      }
    ));
}
