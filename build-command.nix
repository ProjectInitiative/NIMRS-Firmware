{ outputDir ? "build" }:
''
# Ensure we have the partition table
if [ ! -f "partitions.csv" ]; then
  echo "Error: partitions.csv not found in $PWD"
  exit 1
fi

# Get Git Info
GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_VERSION="0.1.0"

echo "Compiling firmware (Version: $BUILD_VERSION, Hash: $GIT_HASH)..."

arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --board-options "FlashSize=8M" \
  --board-options "PartitionScheme=default_8MB" \
  --build-property "build.partitions=$PWD/partitions.csv" \
  --build-property "upload.maximum_size=2097152" \
  --build-property "compiler.cpp.extra_flags=\"-DBUILD_VERSION=\\\