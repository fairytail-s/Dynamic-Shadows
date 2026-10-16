#include "ElfPatch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <cstring>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace shadow::elf {
namespace {
struct SlotPatch {
    void** slot;
    void* original;
    void* detour;
};
std::vector<SlotPatch> gPatches;

int protectionAt(std::uintptr_t address) {
    std::FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f) return PROT_READ;
    char line[4096];
    int prot = 0;
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long long start = 0, end = 0;
        char perms[8] = {};
        if (std::sscanf(line, "%llx-%llx %7s", &start, &end, perms) != 3) continue;
        if (address >= start && address < end) {
            if (perms[0] == 'r') prot |= PROT_READ;
            if (perms[1] == 'w') prot |= PROT_WRITE;
            if (perms[2] == 'x') prot |= PROT_EXEC;
            break;
        }
    }
    std::fclose(f);
    return prot ? prot : PROT_READ;
}

bool writeSlot(void** slot, void* value) {
    const std::uintptr_t page = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE));
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(slot) & ~(page - 1);
    const int oldProtection = protectionAt(reinterpret_cast<std::uintptr_t>(slot));
    if (::mprotect(reinterpret_cast<void*>(start), page, PROT_READ | PROT_WRITE) != 0) return false;
    __atomic_store_n(slot, value, __ATOMIC_RELEASE);
    ::mprotect(reinterpret_cast<void*>(start), page, oldProtection);
    return true;
}

#if defined(__aarch64__)
constexpr unsigned kRelGlobDat = 1025;
constexpr unsigned kRelJumpSlot = 1026;
constexpr unsigned kMachine = EM_AARCH64;
#elif defined(__x86_64__)
constexpr unsigned kRelGlobDat = 6;
constexpr unsigned kRelJumpSlot = 7;
constexpr unsigned kMachine = EM_X86_64;
#else
#error "unsupported architecture"
#endif
}

std::uintptr_t findModuleBase(const char* soName) {
    std::FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[4096];
    std::uintptr_t best = 0;
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long long start = 0, end = 0, offset = 0;
        char perms[8] = {};
        if (std::sscanf(line, "%llx-%llx %7s %llx", &start, &end, perms, &offset) != 4) continue;
        char* path = std::strchr(line, '/');
        if (!path) continue;
        char* newline = std::strpbrk(path, "\r\n");
        if (newline) *newline = '\0';
        const char* slash = std::strrchr(path, '/');
        if (!slash || std::strcmp(slash + 1, soName) != 0) continue;
        if (offset == 0 && (best == 0 || start < best)) best = static_cast<std::uintptr_t>(start);
    }
    std::fclose(f);
    return best;
}

int patchImport(std::uintptr_t base, const char* name, void* detour, std::atomic<void*>& originalOut) {
    const auto* eh = reinterpret_cast<const ElfW(Ehdr)*>(base);
    if (std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64 || eh->e_machine != kMachine) return 0;

    const auto* phdr = reinterpret_cast<const ElfW(Phdr)*>(base + eh->e_phoff);
    std::uintptr_t span = 0;
    const ElfW(Dyn)* dynamic = nullptr;
    for (unsigned i = 0; i < eh->e_phnum; ++i) {
        if (phdr[i].p_type == PT_LOAD) span = std::max<std::uintptr_t>(span, phdr[i].p_vaddr + phdr[i].p_memsz);
        if (phdr[i].p_type == PT_DYNAMIC) dynamic = reinterpret_cast<const ElfW(Dyn)*>(base + phdr[i].p_vaddr);
    }
    if (!dynamic) return 0;

    auto absolute = [&](std::uintptr_t v) { return (v >= base && v < base + span) ? v : base + v; };

    std::uintptr_t jmprel = 0, rela = 0, symtab = 0, strtab = 0;
    std::size_t jmprelSize = 0, relaSize = 0;
    for (const ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_JMPREL: jmprel = absolute(d->d_un.d_ptr); break;
            case DT_PLTRELSZ: jmprelSize = d->d_un.d_val; break;
            case DT_RELA: rela = absolute(d->d_un.d_ptr); break;
            case DT_RELASZ: relaSize = d->d_un.d_val; break;
            case DT_SYMTAB: symtab = absolute(d->d_un.d_ptr); break;
            case DT_STRTAB: strtab = absolute(d->d_un.d_ptr); break;
            default: break;
        }
    }
    if (!symtab || !strtab) return 0;

    int patched = 0;
    auto scan = [&](std::uintptr_t table, std::size_t bytes) {
        if (!table || !bytes) return;
        const auto* entries = reinterpret_cast<const ElfW(Rela)*>(table);
        for (std::size_t i = 0; i < bytes / sizeof(ElfW(Rela)); ++i) {
            const unsigned type = ELF64_R_TYPE(entries[i].r_info);
            if (type != kRelGlobDat && type != kRelJumpSlot) continue;
            const auto* symbol = reinterpret_cast<const ElfW(Sym)*>(symtab) + ELF64_R_SYM(entries[i].r_info);
            const char* symbolName = reinterpret_cast<const char*>(strtab) + symbol->st_name;
            if (std::strcmp(symbolName, name) != 0) continue;

            void** slot = reinterpret_cast<void**>(base + entries[i].r_offset);
            void* current = *slot;
            if (current == detour) continue;
            void* expected = nullptr;
            originalOut.compare_exchange_strong(expected, current);
            if (writeSlot(slot, detour)) {
                gPatches.push_back({slot, current, detour});
                ++patched;
            }
        }
    };
    scan(jmprel, jmprelSize);
    scan(rela, relaSize);
    return patched;
}

void restoreAll() {
    for (const SlotPatch& p : gPatches) {
        if (*p.slot == p.detour) (void)writeSlot(p.slot, p.original);
    }
    gPatches.clear();
}

PatternMatch findUniqueExecutablePattern(const char* module, std::string_view pattern) {
    PatternMatch result;
    const auto base = findModuleBase(module);
    if (!base) return result;
    std::vector<int> bytes;
    std::istringstream input{std::string(pattern)};
    std::string token;
    while (input >> token) {
        if (token == "?" || token == "??") bytes.push_back(-1);
        else {
            char* end = nullptr;
            const long value = std::strtol(token.c_str(), &end, 16);
            if (token.size() != 2 || !end || *end || value < 0 || value > 255) return result;
            bytes.push_back(static_cast<int>(value));
        }
        if (bytes.size() > 512) return result;
    }
    if (bytes.empty()) return result;
    std::size_t anchor = 0, length = 0;
    for (std::size_t i = 0; i < bytes.size();) {
        if (bytes[i] < 0) { ++i; continue; }
        const auto first = i;
        while (i < bytes.size() && bytes[i] >= 0) ++i;
        if (i - first > length) { anchor = first; length = i - first; }
    }
    if (length < 4) return result;
    std::vector<unsigned char> fixed;
    for (std::size_t i = 0; i < length; ++i) fixed.push_back(static_cast<unsigned char>(bytes[anchor + i]));
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> readable;
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return result;
    char line[4096];
    while (std::fgets(line, sizeof(line), maps)) {
        unsigned long long first = 0, last = 0;
        char permissions[8]{};
        if (std::sscanf(line, "%llx-%llx %7s", &first, &last, permissions) == 3 && permissions[0] == 'r')
            readable.emplace_back(static_cast<std::uintptr_t>(first), static_cast<std::uintptr_t>(last));
    }
    std::fclose(maps);
    const auto* header = reinterpret_cast<const ElfW(Ehdr)*>(base);
    if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) || header->e_ident[EI_CLASS] != ELFCLASS64 || header->e_machine != kMachine ||
        header->e_phnum > 256 || header->e_phentsize != sizeof(ElfW(Phdr))) return result;
    const auto* segments = reinterpret_cast<const ElfW(Phdr)*>(base + header->e_phoff);
    for (unsigned i = 0; i < header->e_phnum; ++i) {
        const auto& segment = segments[i];
        if (segment.p_type != PT_LOAD || !(segment.p_flags & PF_X)) continue;
        const std::uintptr_t segmentFirst = base + static_cast<std::uintptr_t>(segment.p_vaddr);
        const std::uintptr_t segmentLast = segmentFirst + static_cast<std::uintptr_t>(segment.p_filesz);
        for (const auto& [first, last] : readable) {
            const std::uintptr_t start = std::max(segmentFirst, first);
            const std::uintptr_t end = std::min(segmentLast, last);
            if (end <= start || end - start < bytes.size()) continue;
            const auto* cursor = reinterpret_cast<const unsigned char*>(start + anchor);
            const auto* limit = reinterpret_cast<const unsigned char*>(end);
            while (cursor < limit) {
                const auto* found = std::search(cursor, limit, fixed.begin(), fixed.end());
                if (found == limit) break;
                const auto address = reinterpret_cast<std::uintptr_t>(found) - anchor;
                cursor = found + 1;
                if ((address & 3u) || address < start || address > end - bytes.size()) continue;
                const auto* candidate = reinterpret_cast<const unsigned char*>(address);
                bool match = true;
                for (std::size_t j = 0; j < bytes.size(); ++j) if (bytes[j] >= 0 && candidate[j] != bytes[j]) { match = false; break; }
                if (!match) continue;
                ++result.matches;
                if (result.matches > 1) { result.address = 0; return result; }
                result.address = address;
            }
        }
    }
    return result;
}
}
