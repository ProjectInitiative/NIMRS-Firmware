{ pkgs, esp-idf }:

pkgs.stdenv.mkDerivation {
  name = "nimrs-firmware-dependencies";

  # We only need the component manifest to determine dependencies
  src = pkgs.runCommand "component-manifest" { } ''
    mkdir -p $out/main
    cp ${../main/idf_component.yml} $out/main/idf_component.yml
    cp ${../sdkconfig.defaults} $out/sdkconfig.defaults
  '';

  nativeBuildInputs = [
    esp-idf
    pkgs.cacert
    pkgs.git
  ];

  # We need to set IDF_TARGET so the component manager knows what to download for
  IDF_TARGET = "esp32s3";

  dontConfigure = true;

  buildPhase = ''
        export HOME=$TMPDIR

        # Create a minimal CMakeLists.txt for idf.py
        cat > CMakeLists.txt <<EOF
    cmake_minimum_required(VERSION 3.16)
    include(\$ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(dependencies)
    EOF

        # Create main component CMakeLists.txt to register the component
        # This ensures idf.py sees 'main' and processes its manifest
        cat > main/CMakeLists.txt <<EOF
    idf_component_register(SRCS "" INCLUDE_DIRS ".")
    EOF

        echo "Downloading dependencies..."
        # We use 'idf.py reconfigure' or component manager directly to resolve and download
        # Since we don't have a full project, we rely on the manifest in main/

        idf.py reconfigure
  '';

  installPhase = ''
    mkdir -p $out
    if [ -d managed_components ]; then
      cp -r managed_components $out/
    else
      echo "Error: managed_components not found!"
      exit 1
    fi

    if [ -f dependencies.lock ]; then
      cp dependencies.lock $out/
    fi
  '';

  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
  # New hash for 3.0.0
  outputHash = "sha256-2kdOWfJdH2DxR1i7e3JnS9bSmcawuRn437Dey7a8xMA=";
}
