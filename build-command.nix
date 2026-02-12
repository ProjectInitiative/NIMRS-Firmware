{
  outputDir ? "build",
}:
''
  # Ensure we have the partition table
  if [ ! -f "partitions.csv" ]; then
    echo "Error: partitions.csv not found in $PWD"
    exit 1
  fi

  # Get Git Info (or fallback to environment variable or "unknown")
  # In Nix build, GIT_HASH_ENV is set. In local shell, we try git command.
  if [ -n "$GIT_HASH_ENV" ] && [ "$GIT_HASH_ENV" != "unknown" ]; then
    GIT_HASH="$GIT_HASH_ENV"
  else
    GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
  fi

  BUILD_VERSION="0.1.0"

  echo "Compiling firmware (Version: $BUILD_VERSION, Hash: $GIT_HASH)..."

  # Note: We escape quotes for Bash so arduino-cli receives them correctly
  arduino-cli compile \
    --fqbn esp32:esp32:esp32s3 \
    --board-options "FlashSize=8M" \
    --board-options "PartitionScheme=default_8MB" \
    --build-property "build.partitions=$PWD/partitions.csv" \
    --build-property "upload.maximum_size=2097152" \
    --build-property "compiler.cpp.extra_flags=-DBUILD_VERSION=\"$BUILD_VERSION\" -DGIT_HASH=\"$GIT_HASH\"" \
    --output-dir "${outputDir}" \
    --warnings default \
    .
''
