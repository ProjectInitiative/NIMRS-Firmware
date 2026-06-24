fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    if !target.contains("esp32s3") {
        return;
    }

    let mut b = cc::Build::new();
    b.compiler("xtensa-esp32s3-elf-gcc");
    b.include("vendor/libhelix");

    // ESP32 platform enables the generic C fall backs in assembly.h
    b.define("ESP32", None);

    // ESP-IDF required compile flags
    b.flag("-Os");
    b.flag("-mlongcalls");
    b.flag("-Wno-unused-variable");
    b.flag("-Wno-unused-function");
    b.flag("-Wno-error");

    // Core helix decoder source files
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
        // Com pat shim that replaces arduino-libhelix's memory management
        "vendor/libhelix/helix_compat.c",
    ];

    for src in &sources {
        b.file(src);
    }

    // Remove the arduino-dependent memory C++ file from include path
    b.compile("libhelix-mp3.a");
}
