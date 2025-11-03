{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
      };
    in
    {
      devShells.${system}.default = pkgs.stdenv.mkDerivation {
        name = "c-shell";

        buildInputs = with pkgs; [
          clang-tools
          clang

          bear

          pkg-config
          pipewire
        ];

        shellHook = ''
          export SHELL=zsh
        '';
      };

      packages.${system} = rec {
        default = mbas;

        mbas = pkgs.stdenv.mkDerivation {
          name = "mbas";

          src = ./.;

          buildInputs = with pkgs; [
            pkg-config
            pipewire
          ];


          buildPhase = "make";
          installPhase = ''
            mkdir -p $out/bin
            cp bin/mbas $out/bin/
          '';
        };
      };
    };
}
