fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();
    if !target.contains("esp32s3") {
        return;
    }

    // Re-emit the ESP-IDF linker arguments that esp-idf-sys's build script
    // propagated. The embuild 0.33.1 crate emits `cargo:rustc-link-arg=`
    // which only applies to lib targets on nightly >= 1.95. We need
    // `cargo:rustc-link-arg-bins=` for the final binary.
    // The env var DEP_ESP_IDF_<LINK_ARGS_VAR> holds the propagated args.
    // embuild uses LINK_ARGS_VAR = "LINK_ARGS"
    if let Ok(link_args) = std::env::var("DEP_ESP_IDF_EMBUILD_LINK_ARGS") {
        // The args are joined with spaces (unix-style by embuild's cli::join_unix_args)
        // We need to split them and emit each one as a separate cargo:rustc-link-arg-bins
        for arg in shell_split(&link_args) {
            println!("cargo:rustc-link-arg-bins={}", arg);
        }
    } else {
        // Fallback: emit the ldproxy linker arg directly
        println!("cargo:rustc-link-arg-bins=--ldproxy-linker=xtensa-esp32s3-elf-gcc");
    }

    // Compile the helix MP3 decoder C sources for Rust FFI
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

/// Simple shell-like argument splitter (handles quotes and spaces)
fn shell_split(s: &str) -> Vec<String> {
    let mut args = Vec::new();
    let mut current = String::new();
    let mut in_quote = false;
    let mut quote_char = '\0';
    for c in s.chars() {
        match c {
            '\'' | '"' if !in_quote => {
                in_quote = true;
                quote_char = c;
            }
            c if in_quote && c == quote_char => {
                in_quote = false;
            }
            ' ' if !in_quote => {
                if !current.is_empty() {
                    args.push(std::mem::take(&mut current));
                }
            }
            _ => current.push(c),
        }
    }
    if !current.is_empty() {
        args.push(current);
    }
    args
}
