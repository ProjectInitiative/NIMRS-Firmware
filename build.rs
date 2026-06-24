fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    if !target.contains("esp32s3") {
        return;
    }

    let out_dir = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    let cc = "xtensa-esp32s3-elf-gcc";
    let ar = "xtensa-esp32s3-elf-ar";

    let sources = [
        "mp3dec.c",
        "bitstream.c",
        "buffers.c",
        "dct32.c",
        "dequant.c",
        "dqchan.c",
        "huffman.c",
        "hufftabs.c",
        "imdct.c",
        "mp3tabs.c",
        "polyphase.c",
        "scalfact.c",
        "stproc.c",
        "subband.c",
        "trigtabs.c",
        "helix_compat.c",
    ];

    let mut objects = Vec::new();
    for src in &sources {
        let src_path = format!("vendor/libhelix/{}", src);
        let obj = out_dir.join(src.replace('/', "_").replace(".c", ".o"));
        let status = std::process::Command::new(cc)
            .args([
                "-Os",
                "-mlongcalls",
                "-Wno-unused-variable",
                "-Wno-unused-function",
                "-Wno-error",
                "-DESP32",
                "-Ivendor/libhelix",
                "-c",
                &src_path,
                "-o",
            ])
            .arg(obj.to_str().unwrap())
            .status()
            .expect("failed to compile helix source");
        if !status.success() {
            panic!("helix compilation failed for {}", src);
        }
        objects.push(obj);
    }

    // Create static library archive manually
    let lib_path = out_dir.join("libhelix-mp3.a");
    let mut ar_cmd = std::process::Command::new(ar);
    ar_cmd.arg("crs").arg(lib_path.to_str().unwrap());
    for obj in &objects {
        ar_cmd.arg(obj.to_str().unwrap());
    }
    let status = ar_cmd.status().expect("failed to create helix archive");
    if !status.success() {
        panic!("ar failed");
    }

    // Use -l: to specify the library directly (avoids -l static= format that confuses ldproxy)
    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-link-lib=helix-mp3");
}
