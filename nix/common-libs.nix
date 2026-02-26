{ pkgsWithArduino, pkgs }:
with pkgsWithArduino.arduinoLibraries;
[
  NmraDcc."2.0.17"
  ArduinoJson."7.3.0"

  (pkgs.fetchFromGitHub {
    owner = "pschatzmann";
    repo = "arduino-audio-tools";
    rev = "217a22e27312a0dd27345d5756f40fe6dab5b30e";
    hash = "sha256-tPvpi8lOwFqsGRkX5f41C4jkZBhkV7JFrhbQZqlqe8c=";
    name = "arduino-audio-tools";
  })

  (pkgs.fetchFromGitHub {
    owner = "pschatzmann";
    repo = "arduino-libhelix";
    rev = "0d70b7f5b4b24f8d7efdfcb2eb9adfa2652f51b8";
    hash = "sha256-aqPOArfVvhGhjJ/up8kFw9xx3R7JyoUvjR6ruPtjoIc=";
    name = "arduino-libhelix";
  })
]
