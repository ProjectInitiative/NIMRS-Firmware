fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    if !target.contains("esp32s3") {
        return;
    }

    let mut b = cc::Build::new();
    b.compiler("xtensa-esp32s3-elf-gcc");
    b.include("vendor/libhelix");

    // Disable assembly optimizations (no Xtensa support in helx assembly.h)
    b.define("NO_ASSEMBLY", None);
    b.define("ARM", None); // Preferred word size hint for helx types

    // ESP-IDF required compile flags
    b.flag("-Os");
    b.flag("-mlongcalls");
    b.flag("-Wno-unused-variable");
    b.flag("-Wno-unused-function");
    b.flag("-Wno-error");
    b.cargo_metadata(true);

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
}
