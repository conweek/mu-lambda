{ pkgs ? import <nixpkgs> {}, ... }:
pkgs.mkShell {
  
    packages = [
        pkgs.gcc
        pkgs.binutils
        pkgs.valgrind
    ];

    shellHook = ''
      '';
}
