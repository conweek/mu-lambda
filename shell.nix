{ pkgs ? import <nixpkgs> {}, ... }:
let
    sshConfig = "Host moss
        Hostname moss.labs.eait.uq.edu.au
        User s4808259 
        IdentityFile ~/.ssh/id_ed25519 
        ForwardAgent yes

Host lichen
        Hostname lichen.labs.eait.uq.edu.au
        User s4808259
        IdentityFile ~/.ssh/id_ed25519 
        ForwardAgent yes

Host cluster 
        Hostname rangpur.compute.eait.uq.edu.au 
        User s4808259 
        IdentityFile ~/.ssh/id_ed25519
        ProxyJump lichen";
in
pkgs.mkShell {
  
    packages = [
        pkgs.gcc
        pkgs.binutils
        pkgs.valgrind
        pkgs.hdf5
        (pkgs.python3.withPackages (ps: [ ps.numpy ps.matplotlib ps.h5py ]))
    ];

    shellHook = ''
      scpr() {
          if [[ $# -lt 3 ]] || ! printf '%s\n' "''${@:2}" | grep -q '^:'; then
            echo "Usage: scpr HOST [:]FILE1 [:]FILE2 ..."
            echo "  Prefix remote paths with ':'"
            echo "  scpr cluster local.txt :/remote/path.txt   # upload"
            echo "  scpr cluster :/remote/path.txt local.txt   # download"
            return 1
          fi
          local host="$1"; shift
          local args=()
          for arg in "$@"; do
            if [[ "$arg" == :* ]]; then
              args+=("''${host}:''${arg#:}")
            else
              args+=("$arg")
            fi
          done
          scp "''${args[@]}"
      }

      alias ssh="kitten ssh"

      export SSHDIR="/home/$(whoami)/.ssh"
      touch $SSHDIR/config
      echo -e "${sshConfig}" >> $SSHDIR/config
      trap "rm -rf $SSHDIR/config" EXIT
      '';
}
