{
  description = "R-Type development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      mkShellFor =
        pkgs: llvm: stdenv:
        let
          inherit (pkgs) lib;

          vcpkg-tool = pkgs.vcpkg-tool.overrideAttrs (old: rec {
            patches = [ ];
            buildInputs = old.buildInputs ++ [ pkgs.curl ];
            cmakeFlags = old.cmakeFlags ++ [ "-DVCPKG_LIBCURL_DLSYM=OFF" ];
            version = "2026-09-26";
            src = pkgs.fetchFromGitHub {
              owner = "microsoft";
              repo = "vcpkg-tool";
              rev = version;
              hash = "sha256-9W+rjsYoZ3jLCI+LwmVWdnqTNowkwoqKT+tw+YFtno8=";
            };
          });

          x11Libs = with pkgs; [
            libx11
            libxcursor
            libxext
            libxi
            libxinerama
            libxrandr
            libxrender
            libxfixes
            libGL
            libGLU
          ];

          linuxOnly = lib.optionals stdenv.hostPlatform.isLinux (
            x11Libs
            ++ (with pkgs; [
              wayland
              libxkbcommon
              alsa-lib
            ])
          );
        in
        pkgs.mkShell.override { inherit stdenv; } {
          packages =
            (with pkgs; [
              cmake
              ninja
              gnumake
              git
              curl
              zip
              unzip
              gnutar
              pkg-config
              autoconf
              autoconf-archive
              automake
              libtool
              jq
              ccache
              vcpkg-tool
              python3Packages.mkdocs-material
              llvm.clang-tools
            ])
            ++ lib.optionals stdenv.hostPlatform.isLinux [
              pkgs.gdb
              pkgs.valgrind
            ];

          buildInputs = linuxOnly;

          hardeningDisable = [ "fortify" ];

          VCPKG_FORCE_SYSTEM_BINARIES = "1";
          VCPKG_DISABLE_METRICS = "1";

          shellHook = ''
            export VCPKG_ROOT="''${RTYPE_VCPKG_ROOT:-''${XDG_CACHE_HOME:-$HOME/.cache}/r-type/vcpkg}"
            export VCPKG_DOWNLOADS="''${VCPKG_DOWNLOADS:-''${XDG_CACHE_HOME:-$HOME/.cache}/r-type/vcpkg-downloads}"
            baseline="$(jq -r '."builtin-baseline"' "$PWD/vcpkg.json" 2>/dev/null || true)"

            if [ ! -d "$VCPKG_ROOT/.git" ]; then
              echo "r-type: cloning vcpkg into $VCPKG_ROOT"
              mkdir -p "$(dirname "$VCPKG_ROOT")"
              git clone --quiet https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
            fi

            if [ -n "$baseline" ]; then
              if ! git -C "$VCPKG_ROOT" cat-file -e "$baseline^{commit}" 2>/dev/null; then
                git -C "$VCPKG_ROOT" fetch --quiet origin
              fi
              git -C "$VCPKG_ROOT" checkout --quiet --detach "$baseline"
            fi

            ln -sfn "$(command -v vcpkg)" "$VCPKG_ROOT/vcpkg"
            mkdir -p "$VCPKG_DOWNLOADS"
            unset baseline
          ''
          + lib.optionalString stdenv.hostPlatform.isLinux ''
            export LD_LIBRARY_PATH="${lib.makeLibraryPath linuxOnly}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
          '';
        };
    in
    {
      devShells = forAllSystems (pkgs: {
        default = mkShellFor pkgs pkgs.llvmPackages_latest pkgs.stdenv;
        clang = mkShellFor pkgs pkgs.llvmPackages_latest pkgs.llvmPackages_latest.stdenv;
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
