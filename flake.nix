{
  description = "NIMRS-Firmware Development Environment (ESP-IDF Native)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    
    # Arduino Indexes for arduino-nix
    arduino-indexes = {
      url = "github:bouk/arduino-indexes";
      flake = false;
    };

    # Arduino Nix with Env patch
    arduino-nix = {
      url = "github:clerie/arduino-nix/clerie/arduino-env";
    };
  };

  outputs = { self, nixpkgs, flake-utils, esp-dev, arduino-indexes, arduino-nix }:
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # ---------------------------------------------------------
        # Arduino Library Handling (Ported from arduino-nix context)
        # ---------------------------------------------------------
        
        # Setup overlays for arduino-nix
        overlays = [
          (arduino-nix.overlay)
          (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_index.json"))
          (arduino-nix.mkArduinoPackageOverlay (arduino-indexes + "/index/package_esp32_index.json"))
          (arduino-nix.mkArduinoLibraryOverlay (arduino-indexes + "/index/library_index.json"))
        ];

        # Create pkgs with Arduino overlays
        pkgsWithArduino = import pkgs.path {
          inherit system overlays;
        };

        # Get libraries from common-libs.nix
        arduinoLibs = import ./common-libs.nix { inherit pkgsWithArduino; };

        # Create a derivation that symlinks these libraries into a structure suitable for IDF components
        # We need to wrap them so they look like components (often need a CMakeLists.txt if missing)
        # For now, we just expose the paths.
        
        setupComponents = pkgs.writeShellScriptBin "setup-components" ''
          echo "Setting up Arduino libraries as IDF components..."
          mkdir -p components

          # Function to link a library
          link_lib() {
            LIB_PATH=$1
            # Get the real library name from library.properties or dirname
            # Since nix store paths have hashes, we strip them.
            LIB_NAME_RAW=$(basename $LIB_PATH)
            LIB_NAME=$(echo "$LIB_NAME_RAW" | sed -E 's/^[a-z0-9]{32}-//')
            
            # Special case mapping for component names in CMake
            COMP_NAME=$(echo "$LIB_NAME" | tr '-' '_' | tr '.' '_')
            
            TARGET_DIR="components/$COMP_NAME"

            if [ -d "$TARGET_DIR" ]; then
              echo "  Skipping $COMP_NAME (already exists in components/)"
              return
            fi

            echo "  Copying $COMP_NAME from $LIB_PATH..."
            # We copy instead of symlink so we can add CMakeLists.txt
            cp -rL "$LIB_PATH" "$TARGET_DIR"
            chmod -R u+w "$TARGET_DIR"

            # Patch ESP8266Audio for IDF 5 compatibility
            if [[ "$COMP_NAME" == "ESP8266Audio_1_9_7" || "$COMP_NAME" == "ESP8266Audio_1_9_9" ]]; then
                echo "    Patching $COMP_NAME for IDF 5..."
                # Fix I2S_MCLK_MULTIPLE_DEFAULT -> I2S_MCLK_MULTIPLE_256 in ALL files
                find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/I2S_MCLK_MULTIPLE_DEFAULT/I2S_MCLK_MULTIPLE_256/g' {} +
                
                # Disable rtc_clk_apll_enable (it's internal/deprecated)
                find "$TARGET_DIR" -type f -name "*.cpp" -exec sed -i 's/rtc_clk_apll_enable/\/\/rtc_clk_apll_enable/g' {} +
            fi

            # Create CMakeLists.txt for the Arduino library
            echo "    Creating CMakeLists.txt for $COMP_NAME..."
            
            # Find the actual library source root (some are nested in libraries/Name/)
            REAL_LIB_ROOT="."
            if [ -d "$TARGET_DIR/libraries" ]; then
                # Find the first directory under libraries/
                NESTED=$(ls "$TARGET_DIR/libraries" | head -n 1)
                if [ -n "$NESTED" ]; then
                    REAL_LIB_ROOT="libraries/$NESTED"
                fi
            fi

            # Determine include dirs and source glob pattern
            INCDIRS="\"$REAL_LIB_ROOT\""
            
            if [ -d "$TARGET_DIR/$REAL_LIB_ROOT/src" ]; then
                INCDIRS="\"$REAL_LIB_ROOT/src\" $INCDIRS"
                # If src exists, only look for sources there (recursively)
                GLOB_PATTERN="GLOB_RECURSE SOURCES \"$REAL_LIB_ROOT/src/*.cpp\" \"$REAL_LIB_ROOT/src/*.c\" \"$REAL_LIB_ROOT/src/*.S\""
            else
                # If no src, look in root (non-recursively to avoid examples/extras)
                GLOB_PATTERN="GLOB SOURCES \"$REAL_LIB_ROOT/*.cpp\" \"$REAL_LIB_ROOT/*.c\" \"$REAL_LIB_ROOT/*.S\""
            fi

            cat > "$TARGET_DIR/CMakeLists.txt" <<EOF
file($GLOB_PATTERN)

if("\''${SOURCES}" STREQUAL "")
    idf_component_register(INCLUDE_DIRS $INCDIRS
                           REQUIRES arduino-esp32)
else()
    idf_component_register(SRCS \''${SOURCES}
                           INCLUDE_DIRS $INCDIRS
                           REQUIRES arduino-esp32)

    # Arduino libraries often need these flags (only for C++)
    target_compile_options(\''${COMPONENT_LIB} PRIVATE 
        \$<\$<COMPILE_LANGUAGE:CXX>:-fno-exceptions> 
        \$<\$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        -Wno-error=stringop-overflow
        -Wno-error=narrowing
    )
endif()
EOF
          }

          ${builtins.concatStringsSep "\n" (map (lib: "link_lib \"${lib}\"") arduinoLibs)}
          
          echo "Done."
        '';

                # The dependency derivation (vendored components)

                nimrsDeps = pkgs.callPackage ./dependencies.nix {

                   esp-idf = esp-dev.packages.${system}.esp-idf-esp32s3;

                };

        

              in

              {

                packages.dependencies = nimrsDeps;

                

                packages.default = pkgs.stdenv.mkDerivation {

                  pname = "nimrs-firmware";

                  version = "0.1.0";

                  src = ./.;

        

                  nativeBuildInputs = [ 

                    esp-dev.packages.${system}.esp-idf-esp32s3 

                    setupComponents

                  ];

        

      

                # We need to set IDF_TARGET

                IDF_TARGET = "esp32s3";

      

                # 1. Setup local components (Arduino libs from common-libs.nix)

                # 2. Link vendored components (IDF Registry libs from dependencies.nix)

                configurePhase = ''

                  export HOME=$TMPDIR

                  

                  # Setup local components

                  setup-components

                  # Ensure config.h exists for the build
                  if [ ! -f main/config.h ]; then
                      echo "main/config.h not found, copying config.example.h..."
                      if [ -f config.example.h ]; then
                          cp config.example.h main/config.h
                      elif [ -f main/config.example.h ]; then
                          cp main/config.example.h main/config.h
                      else
                          echo "Warning: config.example.h not found!"
                      fi
                  fi

                              # Link vendored components

                  

                              cp -r ${nimrsDeps}/managed_components .

                  

                              if [ -f ${nimrsDeps}/dependencies.lock ]; then

                  

                                cp ${nimrsDeps}/dependencies.lock .

                  

                              fi

                  

                              chmod -R u+w managed_components dependencies.lock || true

                  

                            '';

                  

                  

      

                buildPhase = ''

                  idf.py build

                '';

      

                installPhase = ''

                  mkdir -p $out

                  cp build/nimrs-firmware.bin $out/

                  cp build/bootloader/bootloader.bin $out/

                  cp build/partition_table/partition-table.bin $out/

                  cp build/nimrs-firmware.elf $out/

                '';

              };

      

              devShells.default = esp-dev.devShells.${system}.esp-idf-full.overrideAttrs (old: {

      
          buildInputs = old.buildInputs ++ [ setupComponents ];
          shellHook = ''
            ${old.shellHook or ""}
            echo "NIMRS-Firmware ESP-IDF Environment"
            echo "Run 'setup-components' to populate components/ with Arduino libraries."
          '';
        });
      }
    );
}