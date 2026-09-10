use std::env;
use std::path::PathBuf;

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.join("../..");
    let dir = env::var("GENLANG_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| root.join("build"));
    println!("cargo:rerun-if-env-changed=GENLANG_LIB_DIR");
    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=dylib=genlang");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", dir.display());
}
