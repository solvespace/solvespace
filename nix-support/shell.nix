{
  callPackage,
  solvespace-latest ? callPackage ./package.nix { },
  mkShell,
}:
mkShell {
  name = "solvespace";

  inputsFrom = [ solvespace-latest ];
}
