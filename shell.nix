{ pkgs ? import <nixpkgs> {}, ... }:
pkgs.mkShell {
  
    packages = [
        pkgs.gcc
        pkgs.binutils
        pkgs.gdb
        pkgs.valgrind
    ];

    shellHook = ''
      '';
}
