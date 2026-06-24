fn main() {
    // Compile the helix MP3 decoder C sources for Rust FFI
    let mut b = cc::Build::new();
    b.include("vendor/libhelix");
    b.flag("-O2");
    b.flag("-Wno-unused-variable");
    b.flag("-Wno-unused-function");

    let sources = [
        "vendor/libhelix/mp3dec.c",
        "vendor/libhelix/bitstream.c",
        "vendor/libhelix/buffers.c",
        "vendor/libhelix/dct32.c",
        "vendor/libhelix/dequant.c",
        "vendor/libhelix/dqchan.c",
        "vendor/libhelix/huffman.c",
        "vendor/libhelix/hufftabs.c",
        "vendor/libhelix/imdct.c",
        "vendor/libhelix/mp3tabs.c",
        "vendor/libhelix/polyphase.c",
        "vendor/libhelix/scalfact.c",
        "vendor/libhelix/stproc.c",
        "vendor/libhelix/subband.c",
        "vendor/libhelix/trigtabs.c",
    ];

    for src in &sources {
        b.file(src);
    }

    b.compile("libhelix-mp3.a");
    println!("cargo:rustc-link-lib=helix-mp3");
}
