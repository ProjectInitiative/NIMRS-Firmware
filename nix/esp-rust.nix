{
  stdenv,
  fetchurl,
  cacert,
  autoPatchelfHook,
  xz,
  makeWrapper,
}:

let
  espup-url = "https://github.com/esp-rs/espup/releases/download/v0.17.1/espup-x86_64-unknown-linux-gnu";
  espup-hash = "sha256-2+VOmQe2h4CdvhuVVzFWntbfK1JTYnENZ2JWxcjPnM0=";

  espup-bin = stdenv.mkDerivation {
    name = "espup-bin";
    src = fetchurl {
      url = espup-url;
      sha256 = espup-hash;
    };
    nativeBuildInputs = [
      autoPatchelfHook
      makeWrapper
    ];
    buildInputs = [
      stdenv.cc.cc.lib
      xz
    ];
    dontUnpack = true;
    installPhase = ''
      mkdir -p $out/bin
      cp $src $out/bin/espup
      chmod +x $out/bin/espup
      wrapProgram $out/bin/espup --set SSL_CERT_FILE ${cacert}/etc/ssl/certs/ca-bundle.crt
    '';
  };
in
stdenv.mkDerivation {
  name = "esp-xtensa-rust-toolchain";

  nativeBuildInputs = [
    espup-bin
    cacert
  ];

  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
  outputHash = "";
  __noChroot = true;

  buildCommand = ''
    export HOME=$NIX_BUILD_TOP/home
    export RUSTUP_HOME=$HOME/.rustup
    export CARGO_HOME=$HOME/.cargo
    mkdir -p $RUSTUP_HOME $CARGO_HOME $out

    echo "espup: downloading Xtensa Rust toolchain (this will take several minutes)..."
    espup install \
      --targets esp32s3 \
      --export-file /tmp/export-esp.sh

    echo "espup: toolchain installed"
    ls -la $RUSTUP_HOME/toolchains/esp/bin/ 2>/dev/null | head -15

    if [ -d "$RUSTUP_HOME/toolchains/esp" ]; then
      cp -r $RUSTUP_HOME/toolchains/esp/* $out/
    else
      echo "ERROR: espup did not create expected toolchain directory"
      find $HOME -type d 2>/dev/null | head -30
      exit 1
    fi
  '';
}
