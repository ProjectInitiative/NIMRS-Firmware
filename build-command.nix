{ outputDir ? "build" }:
''
# Ensure we have the partition table
if [ ! -f "partitions.csv" ]; then
  echo "Error: partitions.csv not found in $PWD"
  exit 1
fi

echo "Compiling firmware..."
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --board-options "FlashSize=8M" \
  --board-options "PartitionScheme=default_8MB" \
  --build-property "build.partitions=$PWD/partitions.csv" \
  --build-property "upload.maximum_size=2097152" \
  --output-dir "${outputDir}" \
  --warnings default \
  .
''
