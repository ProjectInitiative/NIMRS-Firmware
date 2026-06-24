fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    if !target.contains("esp32s3") {
        return;
    }

    let out_dir = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    let cc = "xtensa-esp32s3-elf-gcc";

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

    let mut objs = Vec::new();
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
            .expect("helix compile failed");
        assert!(status.success(), "helix compile failed for {}", src);
        objs.push(obj);
    }

    // Combine into a single relocatable object
    let combined = out_dir.join("helix.o");
    let mut ld_cmd = std::process::Command::new("xtensa-esp32s3-elf-ld");
    ld_cmd.arg("-r").arg("-o").arg(combined.to_str().unwrap());
    for o in &objs {
        ld_cmd.arg(o.to_str().unwrap());
    }
    let status = ld_cmd.status().expect("ld -r failed");
    assert!(status.success(), "ld -r failed");

    println!("cargo:rustc-link-arg-bins={}", combined.to_str().unwrap());
}
