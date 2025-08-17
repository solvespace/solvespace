{
  nixpkgs ? <nixpkgs>,
  pkgs ? import nixpkgs { overlays = [(import ./nix-support/overlay.nix)];}
}:
pkgs.solvespace-latest
