{ pkgs, arduinoLibs }:

pkgs.stdenv.mkDerivation {
  name = "arduino-components";

  phases = [ "installPhase" ];

  installPhase = ''
        mkdir -p $out
        
        link_lib() {
          LIB_PATH=$1
          LIB_NAME_RAW=$(basename $LIB_PATH)
          LIB_NAME=$(echo "$LIB_NAME_RAW" | sed -E 's/^[a-z0-9]{32}-//')
          GENERIC_NAME=$(echo "$LIB_NAME" | sed -E 's/[-.][0-9].*$//')
          COMP_NAME=$(echo "$GENERIC_NAME" | tr '-' '_' | tr '.' '_')
          TARGET_DIR="$out/$COMP_NAME"

                echo "Processing $COMP_NAME from $LIB_PATH..."
                cp -rL "$LIB_PATH" "$TARGET_DIR"
                chmod -R u+w "$TARGET_DIR"
          
                REAL_LIB_ROOT="."
      if [ -d "$TARGET_DIR/libraries" ]; then
              NESTED=$(ls "$TARGET_DIR/libraries" | head -n 1)
              if [ -n "$NESTED" ]; then
                  REAL_LIB_ROOT="libraries/$NESTED"
              fi
          fi

          if [ -d "$TARGET_DIR/$REAL_LIB_ROOT/src" ]; then
              INCDIRS="\"$REAL_LIB_ROOT/src\" \"$REAL_LIB_ROOT\""
              cat > "$TARGET_DIR/CMakeLists.txt" <<'EOF'
    file(GLOB_RECURSE SOURCES "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/src/*.cpp" "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/src/*.c" "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/src/*.S")
    EOF
          else
              INCDIRS="\"$REAL_LIB_ROOT\""
              cat > "$TARGET_DIR/CMakeLists.txt" <<'EOF'
    file(GLOB SOURCES "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/*.cpp" "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/*.c" "''${CMAKE_CURRENT_LIST_DIR}/@REAL_LIB_ROOT@/*.S")
    EOF
          fi

          cat >> "$TARGET_DIR/CMakeLists.txt" <<'EOF'
    if("''${SOURCES}" STREQUAL "")
        idf_component_register(INCLUDE_DIRS @INCDIRS@ REQUIRES espressif__arduino-esp32)
    else()
        idf_component_register(SRCS ''${SOURCES} INCLUDE_DIRS @INCDIRS@ REQUIRES espressif__arduino-esp32)
        target_compile_options(''${COMPONENT_LIB} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
            -Wno-error=stringop-overflow
            -Wno-error=narrowing
        )
    endif()
    EOF
          sed -i "s|@REAL_LIB_ROOT@|$REAL_LIB_ROOT|g" "$TARGET_DIR/CMakeLists.txt"
          sed -i "s|@COMP_NAME@|$COMP_NAME|g" "$TARGET_DIR/CMakeLists.txt"
          sed -i "s|@INCDIRS@|$INCDIRS|g" "$TARGET_DIR/CMakeLists.txt"
        }

        ${pkgs.lib.concatStringsSep "\n" (
          map (lib: ''
            link_lib "${lib}"
          '') arduinoLibs
        )}
  '';
}
