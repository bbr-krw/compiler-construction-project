{
  description = "D interpreter development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          name = "d-interpreter-dev";

          packages = with pkgs; [
            gcc
            gdb
            cmake
            bison
            gnumake
            clang-tools
            gtest           # Added Google Test
            gtest.dev       # Added Google Test development files (headers, etc.)
          ];

          shellHook = ''
            echo "D interpreter dev shell"
            echo "  gcc   $(gcc --version | head -1)"
            echo "  gdb   $(gdb --version | head -1)"
            echo "  cmake $(cmake --version | head -1)"
            echo "  bison $(bison --version | head -1)"
          '';

          CMAKE_CXX_COMPILER="${pkgs.gcc}/bin/g++";
          CMAKE_CC_COMPILER="${pkgs.gcc}/bin/gcc";
        };
      }
    );
}
