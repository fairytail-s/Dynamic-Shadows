import csv
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "lightdocs" / "reference"


def write_csv(name, columns, rows):
    with (DESTINATION / name).open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(columns)
        writer.writerows(rows)


def main():
    DESTINATION.mkdir(parents=True, exist_ok=True)
    hooks = []
    aliases = []
    for relative in ["src/GlHooks.cpp", "src/DrawBatch.cpp"]:
        source = (ROOT / relative).read_text(encoding="utf-8")
        for name in re.findall(r'\{"(gl\w+)", reinterpret_cast<void\*>', source):
            if name.startswith("glDraw") and "Indirect" not in name:
                role = "original draw and caster capture"
            elif name == "glUniformBlockBinding":
                role = "uniform-block association guard and binding-map invalidation"
            elif "Uniform" in name:
                role = "camera capture" if relative.endswith("GlHooks.cpp") else "ordinary uniform cache and upload journal"
            elif name in ["glActiveTexture", "glBindTexture", "glBindSampler", "glBindVertexArray", "glCullFace", "glFrontFace", "glPolygonOffset", "glDepthRangef"]:
                role = "binding or pipeline mirror"
            elif relative.endswith("GlHooks.cpp"):
                role = "classification state or frame trigger"
            else:
                role = "resource lifetime/content guard or scope boundary"
            hooks.append([name, relative, role, "ELF import or eglGetProcAddress", "runtime", ""])
        for alias, canonical in re.findall(r'if \(!std::strcmp\(name, "([^"]+)"\)\) canonical = "([^"]+)";', source):
            aliases.append([alias, canonical, relative])
    hooks.extend([["eglGetProcAddress", "src/GlHooks.cpp", "dynamic GL lookup interception", "ELF import; inline fallback", "runtime", ""],
                  ["eglSwapBuffers", "src/GlHooks.cpp", "frame boundary", "ELF import; inline fallback/watchdog", "runtime", ""]])
    write_csv("gl-hooks.csv", ["symbol", "source", "role", "resolution", "address_kind", "fixed_rva"], hooks)
    write_csv("gl-aliases.csv", ["alias", "canonical_symbol", "source"], aliases)
    binary_path = DESTINATION / "android-binary.json"
    binary = json.loads(binary_path.read_text(encoding="utf-8"))["new"] if binary_path.exists() else {}
    source = (ROOT / "src/GameTime.cpp").read_text(encoding="utf-8")
    pattern = re.search(r'kGetTimeSignature\s*=\s*"([?A-Fa-f0-9 ]+)"', source).group(1)
    matches = binary.get("get_time_matches", [])
    version = ";".join(binary.get("version_strings", []))
    rva = matches[0] if len(matches) == 1 else ""
    write_csv("signatures.csv", ["id", "module", "abi", "optional", "required_for_shadows", "pattern", "current_rva", "aligned_executable_matches", "binary_sha256", "version", "evidence"],
              [["LevelGetTime", "libminecraftpe.so", "int(const Level*) / AArch64", "true", "false", pattern, rva, len(matches), binary.get("sha256", ""), version,
                "Unique pattern candidate in supplied ELF; semantic identity inherited from working original source; no device verification"]])
    addresses = [["Level::getTime", "Minecraft function", "optional unique executable pattern", rva, version, binary.get("sha256", ""), "No runtime hardcoded RVA"]]
    addresses += [[row[0], "GL/EGL API", row[3], "runtime", "device driver", "", "No Minecraft object offset"] for row in hooks]
    write_csv("runtime-addresses.csv", ["name", "kind", "discovery", "rva_or_address", "build", "binary_sha256", "note"], addresses)
    write_csv("offsets.csv", ["name", "kind", "value", "source", "discovery", "note"], [])
    write_csv("constants.csv", ["name", "value", "unit", "purpose", "minecraft_layout_offset"], [
        ["scene depth texture unit", 15, "GL texture unit", "screen composite input", "false"],
        ["shadow texture unit", 13, "GL texture unit", "screen composite input", "false"],
        ["caster margin", 40, "blocks", "conservative cascade rejection", "false"],
        ["uniform append guard", 8 * 1024 * 1024, "bytes", "split command capture", "false"],
        ["batch queue split", 4 * 1024 * 1024, "approximate bytes", "split before next caster", "false"],
        ["cached private vertex layouts", 4096, "VAOs per EGL context", "reuse geometry layouts without framebuffer switches", "false"],
        ["cached vertex attribute slots", 32, "attributes", "direct indexed CPU layout cache", "false"],
        ["world composite variants", 4, "shader programs per context", "cascade and filtering specialization", "false"],
        ["hand composite variants", 2, "shader programs per context", "filtering specialization", "false"],
        ["UNIFORM_BUFFER", "0x8A11", "GL enum", "read-only indexed buffer snapshots", "false"],
        ["UNIFORM_BUFFER_BINDING", "0x8A28", "GL enum", "generic and indexed binding queries", "false"],
        ["UNIFORM_BUFFER_START", "0x8A29", "GL enum", "indexed range offset query", "false"],
        ["UNIFORM_BUFFER_SIZE", "0x8A2A", "GL enum", "indexed range size; zero denotes whole-buffer binding", "false"],
        ["ACTIVE_UNIFORM_BLOCKS", "0x8A36", "GL enum", "program block reflection", "false"],
        ["UNIFORM_BLOCK_BINDING", "0x8A3F", "GL enum", "active block binding-point query", "false"],
        ["R_AARCH64_GLOB_DAT", 1025, "ELF relocation kind", "GOT lookup", "false"],
        ["R_AARCH64_JUMP_SLOT", 1026, "ELF relocation kind", "GOT lookup", "false"]])
    settings = []
    config_source = (ROOT / "include/Config.hpp").read_text(encoding="utf-8")
    expression = r'(Bool|Float|Int)Setting\{"([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", &RuntimeConfig::(\w+), ([^}]+)\}'
    for kind, key, title, description, category, section, field, raw in re.findall(expression, config_source):
        numbers = [value.strip().removesuffix("f") for value in raw.split(",")]
        if kind == "Bool":
            default, minimum, maximum, step = numbers[0], "", "", ""
        else:
            default, minimum, maximum, step = numbers
        scale = 100 if field in ["strength", "nearFraction", "fadeStart", "tintRed", "tintGreen", "tintBlue"] else 1
        control = "toggle" if kind == "Bool" else "choice" if field in ["shadowRes", "filterQuality", "depthBits", "updateInterval"] else "slider"
        settings.append([key, kind.lower(), default, minimum, maximum, step, category, section, scale, control, field, title, description])
    write_csv("settings.csv", ["key", "type", "default", "min", "max", "step", "category", "section", "display_scale", "menu_control", "runtime_field", "title", "description"], settings)
    menu = (ROOT / "src/Menu.cpp").read_text(encoding="utf-8")
    prefix = re.search(r'std::string s = R"\((.*?)\)";', menu, re.S).group(1)
    categories = json.loads(prefix + "]}")["categories"]
    write_csv("menu-categories.csv", ["id", "title", "description"], [[entry["id"], entry["title"], entry["description"]] for entry in categories])
    section_source = menu.split("constexpr Section sections[]{", 1)[1].split("};", 1)[0]
    write_csv("menu-sections.csv", ["id", "title", "category"], re.findall(r'\{"([^"]+)", "([^"]+)", "([^"]+)"\}', section_source))
    write_csv("camera-uniforms.csv", ["name", "meaning", "light_replacement", "discovery"], [
        ["u_view", "camera view", "light view", "GetUniformLocation"],
        ["u_proj", "camera projection", "light orthographic projection", "GetUniformLocation"],
        ["u_viewProj", "camera projection times view", "light projection times view", "GetUniformLocation"],
        ["u_modelView", "camera view times model", "light view times inverse camera view times old model-view", "GetUniformLocation"],
        ["u_modelViewProj", "camera projection times view times model", "light VP times inverse camera VP times old MVP", "GetUniformLocation"]])
    print(f"{len(hooks)} GL/EGL hook symbols; {len(aliases)} aliases; {len(settings)} configurable fields; 1 optional game signature; 0 Minecraft object offsets")


if __name__ == "__main__":
    main()
