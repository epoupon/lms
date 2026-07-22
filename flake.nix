{
  description = "LMS (Lightweight Music Server) development flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;

        mkLms =
          {
            runTests ? false,
          }:
          pkgs.stdenv.mkDerivation {
            pname = "lms";
            version = "git-${self.shortRev or "dirty"}";

            src = lib.cleanSourceWith {
              src = ./.;
              filter =
                path: type:
                let
                  relPath = lib.removePrefix "${toString ./.}/" (toString path);
                in
                lib.cleanSourceFilter path type
                && relPath != "build"
                && !(lib.hasPrefix "build/" relPath)
                && relPath != ".lms-workdir"
                && !(lib.hasPrefix ".lms-workdir/" relPath);
            };
            strictDeps = true;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.patchelf
              pkgs.pkg-config
            ];

            buildInputs = [
              pkgs.gtest
              pkgs.boost
              pkgs.wt
              pkgs.taglib
              pkgs.libconfig
              pkgs.libarchive
              pkgs.ffmpeg
              pkgs.zlib
              pkgs.libSM
              pkgs.libICE
              pkgs.stb
              pkgs.openssl
              pkgs.xxHash
              pkgs.pugixml
            ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DCMAKE_UNITY_BUILD=ON"
              "-DLMS_IMAGE_BACKEND=stb"
              "-DENABLE_TESTS=${if runTests then "ON" else "OFF"}"
              "-DBUILD_TESTING=${if runTests then "ON" else "OFF"}"
            ];

            postPatch = ''
              substituteInPlace src/libs/core/include/core/SystemPaths.hpp --replace-fail "/etc" "$out/share/lms"
            '';

            postInstall = ''
              substituteInPlace $out/share/lms/lms.conf --replace-fail "/usr/bin/ffmpeg" "${lib.getExe pkgs.ffmpeg}"
              substituteInPlace $out/share/lms/lms.conf --replace-fail "/usr/share/Wt/resources" "${pkgs.wt}/share/Wt/resources"
              substituteInPlace $out/share/lms/lms.conf --replace-fail "/usr/share/lms/docroot" "$out/share/lms/docroot"
              substituteInPlace $out/share/lms/lms.conf --replace-fail "/usr/share/lms/approot" "$out/share/lms/approot"
              substituteInPlace $out/share/lms/default.service --replace-fail "/usr/bin/lms" "$out/bin/lms"
            '';

            # Wt's graphics stack pulls these in transitively.  They are not
            # direct linker inputs of lms, so Nix's normal rpath setup omits
            # them unless they are added explicitly.
            postFixup = ''
              patchelf --add-rpath '${lib.makeLibraryPath [ pkgs.libSM pkgs.libICE ]}' $out/bin/lms
            '';

            doCheck = runTests;
            checkPhase = ''
              runHook preCheck
              ctest --output-on-failure -j"$NIX_BUILD_CORES"
              runHook postCheck
            '';
          };

        lms = mkLms { };
        lmsChecks = mkLms { runTests = true; };

        lmsDevRunner = pkgs.writeShellApplication {
          name = "lms-dev";
          runtimeInputs = [ pkgs.coreutils ];
          text = ''
                        set -euo pipefail

                        workdir="''${LMS_WORKDIR:-$PWD/.lms-workdir}"
                        mkdir -p "$workdir"
                        config="$workdir/lms.conf"

                        cat > "$config" <<EOF
            working-dir = "$workdir";
            ffmpeg-file = "${lib.getExe pkgs.ffmpeg}";
            log-file = "";
            listen-port = ''${LMS_PORT:-5082};
            listen-addr = "''${LMS_LISTEN_ADDR:-127.0.0.1}";
            wt-resources = "${pkgs.wt}/share/Wt/resources";
            docroot = "${lms}/share/lms/docroot/;/resources,/css,/images,/js,/favicon.ico";
            approot = "${lms}/share/lms/approot";
            deploy-path = "/";
            authentication-backend = "internal";
            api-subsonic = true;
            api-subsonic-support-user-password-auth = true;
            EOF

                        exec "${lms}/bin/lms" "$config"
          '';
        };
      in
      {
        packages = {
          default = lms;
          lms = lms;
        };

        checks = {
          default = lmsChecks;
          lms = lmsChecks;
        };

        apps = {
          default = {
            type = "app";
            program = "${lmsDevRunner}/bin/lms-dev";
          };
          lms = {
            type = "app";
            program = "${lmsDevRunner}/bin/lms-dev";
          };
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            boost
            wt
            taglib
            libconfig
            libarchive
            ffmpeg
            zlib
            libSM
            libICE
            stb
            openssl
            xxHash
            pugixml
            gtest
            clang-tools
          ];

          shellHook = ''
            echo "LMS development shell"
            echo "Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_UNITY_BUILD=ON -DLMS_IMAGE_BACKEND=stb"
            echo "Run tests: ctest --test-dir build --output-on-failure"
            echo "Run app: nix run .#lms"
          '';
        };
      }
    );
}
