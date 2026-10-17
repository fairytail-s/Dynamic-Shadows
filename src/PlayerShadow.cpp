#include "PlayerShadow.hpp"

#include "Config.hpp"
#include "ElfPatch.hpp"

#include <pl/memory/Hook.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <cstdint>

namespace shadow::player {
namespace {
using ActorPassFn = void (*)(void*);
using CollectorInitFn = void (*)(void*, void*, bool);
using RenderFn = void (*)(void*, void*, void*, bool);
using ContextFn = void* (*)(void*);
using PerspectiveFn = int (*)(void*);
using VariableFn = void* (*)(void*, std::uint64_t, const char*);
using NumberFn = float* (*)(void*);
using MeshFn = void (*)(void*, void*, void*, void*, std::uint16_t, void*, void*, void*, const void*, void*, unsigned char, bool);
using ClipFn = void (*)(void*, const void*, const void*);
using ScissorFn = std::uint16_t (*)(void*, std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t);

std::atomic<void*> originalPass{}, originalCollectorInit{}, originalMesh{}, originalClip{};
RenderFn renderActor{};
std::atomic_bool active{}, rendererReady{}, expected{};
std::array<void*, 4> targets{};
ContextFn getClientLocalPlayer{}, getOptions{}, getVariables{}, getScreen{};
PerspectiveFn getPerspective{};
VariableFn getVariable{};
NumberFn getNumber{};
ScissorFn setScissor{};

constexpr std::array<float, 4> kCasterClip{0x1.013p-5f, 0x1.027p-4f, 0x1.efddp-1f, 0x1.bfb3p-1f};
thread_local bool renderingCaster{};

bool wanted() {
    const auto& c = config();
    return active.load(std::memory_order_acquire) && rendererReady.load(std::memory_order_acquire) &&
           c.enabled.load(std::memory_order_relaxed) &&
           (c.strength.load(std::memory_order_relaxed) > 0.001f || c.debugView.load(std::memory_order_relaxed));
}

void* getLocalPlayer(void* context) {
    void* client = context ? *reinterpret_cast<void**>(static_cast<unsigned char*>(context) + 0x18) : nullptr;
    return client ? getClientLocalPlayer(client) : nullptr;
}

bool isFirstPerson(void* context) {
    void* options = getOptions(context);
    return options && getPerspective(options) == 0;
}

struct ActorPassState {
    void* context{};
    bool submitted{};
};
thread_local ActorPassState* currentPass{};

struct PassScope {
    ActorPassState state;
    ActorPassState* previous;
    explicit PassScope(void* context) : state{context, false}, previous(currentPass) { currentPass = &state; }
    ~PassScope() { currentPass = previous; }
};

struct CasterScope {
    float& value;
    float saved;
    unsigned char* clip;
    std::array<unsigned char, 20> previousClip;
    unsigned char* actorFlags;
    std::array<unsigned char, 3> previousFlags;
    float& ditherDistance;
    float previousDitherDistance;
    bool previous;
    CasterScope(float& number, void* screen, void* context)
        : value(number), saved(number), clip(static_cast<unsigned char*>(screen) + 0x68),
          actorFlags(static_cast<unsigned char*>(context) + 0xb8),
          ditherDistance(*reinterpret_cast<float*>(static_cast<unsigned char*>(context) + 0xb4)),
          previousDitherDistance(ditherDistance), previous(renderingCaster) {
        std::memcpy(previousClip.data(), clip, previousClip.size());
        std::memcpy(previousFlags.data(), actorFlags, previousFlags.size());
        actorFlags[0] = 1;
        actorFlags[1] = 1;
        actorFlags[2] = 0;
        ditherDistance = -1.0f;
        std::memcpy(clip, kCasterClip.data(), sizeof(kCasterClip));
        clip[16] = 1;
        if (saved != 0.0f) value = 0.0f;
        renderingCaster = true;
    }
    ~CasterScope() {
        renderingCaster = previous;
        std::memcpy(clip, previousClip.data(), previousClip.size());
        std::memcpy(actorFlags, previousFlags.data(), previousFlags.size());
        ditherDistance = previousDitherDistance;
        value = saved;
    }
};

void actorPassDetour(void* self) {
    auto original = reinterpret_cast<ActorPassFn>(originalPass.load(std::memory_order_acquire));
    if (!original) return;
    if (!self || !wanted()) {
        expected.store(false, std::memory_order_relaxed);
        original(self);
        return;
    }
    void* context = *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + 0x10);
    void* actor = getLocalPlayer(context);
    const bool firstPerson = actor && isFirstPerson(context);
    expected.store(firstPerson, std::memory_order_relaxed);
    PassScope scope(firstPerson ? context : nullptr);
    original(self);
}

void collectorInitDetour(void* self, void* renderer, bool enabled) {
    auto original = reinterpret_cast<CollectorInitFn>(originalCollectorInit.load(std::memory_order_acquire));
    if (!original) return;
    original(self, renderer, enabled);
    auto* pass = currentPass;
    if (!pass || !pass->context || pass->submitted || renderingCaster || !wanted()) return;
    pass->submitted = true;
    void* context = pass->context;
    void* actor = getLocalPlayer(context);
    if (!actor || !isFirstPerson(context)) return;
    void* dispatcher = *reinterpret_cast<void**>(static_cast<unsigned char*>(context) + 0x38);
    void* screen = getScreen(context);
    void* variables = getVariables(actor);
    void* variable = variables ? getVariable(variables, 0x2739f381184de4aeULL, "variable.is_first_person") : nullptr;
    float* number = variable ? getNumber(variable) : nullptr;
    if (!dispatcher || !screen || !number || !std::isfinite(*number)) return;
    CasterScope scope(*number, screen, context);
    renderActor(dispatcher, context, actor, false);
}

void meshDetour(void* self, void* screen, void* data, void* material, std::uint16_t group,
                void* textures, void* resources, void* deferred, const void* transform,
                void* extra, unsigned char flags, bool environmental) {
    auto original = reinterpret_cast<MeshFn>(originalMesh.load(std::memory_order_acquire));
    if (!original) return;
    original(self, screen, data, material, group, textures, resources, renderingCaster ? nullptr : deferred,
             transform, extra, flags, environmental);
}

void clipDetour(void* self, const void* viewport, const void* region) {
    auto original = reinterpret_cast<ClipFn>(originalClip.load(std::memory_order_acquire));
    if (!original) return;
    const auto* bytes = static_cast<const unsigned char*>(region);
    if (self && bytes && bytes[0] && !std::memcmp(bytes + 4, kCasterClip.data(), sizeof(kCasterClip))) {
        void* renderer = *static_cast<void**>(self);
        if (renderer) {
            void* encoder = *reinterpret_cast<void**>(static_cast<unsigned char*>(renderer) + 0x10);
            if (encoder) {
                setScissor(encoder, kCasterScissorX, kCasterScissorY, kCasterScissorSize, kCasterScissorSize);
                return;
            }
        }
    }
    original(self, viewport, region);
}

bool readable(std::uintptr_t address, std::size_t length, bool executable = false) {
    if (!length || address + length < address) return false;
    std::FILE* file = std::fopen("/proc/self/maps", "r");
    if (!file) return false;
    char line[4096];
    bool found = false;
    while (std::fgets(line, sizeof(line), file)) {
        unsigned long long first = 0, last = 0;
        char permissions[8]{};
        if (std::sscanf(line, "%llx-%llx %7s", &first, &last, permissions) == 3 && permissions[0] == 'r' &&
            (!executable || permissions[2] == 'x') && address >= first && address + length <= last) {
            found = true;
            break;
        }
    }
    std::fclose(file);
    return found;
}

bool supportedBuild(std::uintptr_t base) {
    constexpr unsigned char buildId[] = {
        0x56, 0xde, 0x9e, 0xed, 0x07, 0x76, 0x31, 0xe0, 0x3a, 0x31,
        0xf4, 0xf5, 0x8e, 0xb2, 0xf0, 0xe0, 0x00, 0x71, 0xd3, 0x35
    };
    if (!readable(base, sizeof(Elf64_Ehdr))) return false;
    const auto* header = reinterpret_cast<const Elf64_Ehdr*>(base);
    if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) || header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_machine != EM_AARCH64 || header->e_phnum > 256 || header->e_phentsize != sizeof(Elf64_Phdr) ||
        !readable(base + header->e_phoff, header->e_phnum * sizeof(Elf64_Phdr))) return false;
    const auto* segments = reinterpret_cast<const Elf64_Phdr*>(base + header->e_phoff);
    for (unsigned i = 0; i < header->e_phnum; ++i) {
        const auto& segment = segments[i];
        if (segment.p_type != PT_NOTE || segment.p_memsz > 65536 ||
            !readable(base + segment.p_vaddr, segment.p_memsz)) continue;
        const auto* cursor = reinterpret_cast<const unsigned char*>(base + segment.p_vaddr);
        const auto* end = cursor + segment.p_memsz;
        while (static_cast<std::size_t>(end - cursor) >= sizeof(Elf64_Nhdr)) {
            Elf64_Nhdr note;
            std::memcpy(&note, cursor, sizeof(note));
            cursor += sizeof(note);
            const auto nameBytes = (std::size_t(note.n_namesz) + 3u) & ~std::size_t(3);
            const auto valueBytes = (std::size_t(note.n_descsz) + 3u) & ~std::size_t(3);
            if (nameBytes > static_cast<std::size_t>(end - cursor) || valueBytes > static_cast<std::size_t>(end - cursor) - nameBytes) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && note.n_descsz == sizeof(buildId) &&
                !std::memcmp(cursor, "GNU", 4) && !std::memcmp(cursor + nameBytes, buildId, sizeof(buildId))) return true;
            cursor += nameBytes + valueBytes;
        }
    }
    return false;
}

struct Entry {
    std::uintptr_t rva;
    std::size_t offset;
    const unsigned char* bytes;
    std::size_t length;
};
constexpr unsigned char kActorPass[] = {0xf8, 0x5f, 0x05, 0xa9, 0xf6, 0x57, 0x06, 0xa9, 0xf4, 0x4f, 0x07, 0xa9, 0xfd, 0x83, 0x00, 0x91, 0xff, 0xc3, 0x07, 0xd1, 0x54, 0xd0, 0x3b, 0xd5, 0xe1, 0x00, 0x80, 0x52, 0x88, 0x16, 0x40, 0xf9, 0xa8, 0x03, 0x1d, 0xf8, 0x15, 0xcc, 0x40, 0xa9, 0xe0, 0x03, 0x04, 0x91, 0xd8, 0x69, 0x13, 0x95};
constexpr unsigned char kCollectorInit[] = {0x01, 0x00, 0x00, 0xf9, 0x02, 0x20, 0x00, 0x39, 0x1f, 0x24, 0x00, 0x39, 0x1f, 0x28, 0x00, 0xf9, 0x00, 0x80, 0x00, 0xad, 0x00, 0x80, 0x01, 0xad, 0x00, 0x60, 0x01, 0x91, 0xe3, 0x2f, 0x0c, 0x94, 0x60, 0x02, 0x40, 0xf9, 0xc0, 0x00, 0x00, 0xb4};
constexpr unsigned char kRender[] = {0xf6, 0x57, 0x07, 0xa9, 0xf4, 0x4f, 0x08, 0xa9, 0xfd, 0x43, 0x01, 0x91, 0x57, 0xd0, 0x3b, 0xd5, 0xf5, 0x03, 0x01, 0xaa, 0xf6, 0x03, 0x00, 0xaa, 0xe8, 0x16, 0x40, 0xf9};
constexpr unsigned char kLocalPlayer[] = {0xe8, 0x03, 0x00, 0xaa, 0xe0, 0x23, 0x00, 0x91, 0x69, 0x16, 0x40, 0xf9, 0x01, 0xa1, 0x08, 0x91, 0xa9, 0x83, 0x1f, 0xf8, 0xb4, 0xea, 0x32, 0x95, 0xe0, 0x23, 0x00, 0x91, 0xb3, 0xea, 0x32, 0x95, 0xa0, 0x00, 0x00, 0x36, 0xe0, 0x23, 0x00, 0x91, 0x21, 0x00, 0x80, 0x52, 0x6f, 0x28, 0x4b, 0x94};
constexpr unsigned char kOptions[] = {0x00, 0x0c, 0x40, 0xf9, 0x08, 0x00, 0x40, 0xf9, 0x01, 0xbd, 0x42, 0xf9, 0x20, 0x00, 0x1f, 0xd6};
constexpr unsigned char kPerspective[] = {0xfd, 0x7b, 0xbf, 0xa9, 0xfd, 0x03, 0x00, 0x91, 0x08, 0x00, 0x40, 0xf9, 0x61, 0x00, 0x80, 0x52, 0x08, 0x09, 0x40, 0xf9, 0x00, 0x01, 0x3f, 0xd6, 0xfd, 0x7b, 0xc1, 0xa8, 0xd3, 0x17, 0x77, 0x14};
constexpr unsigned char kVariables[] = {0x00, 0x40, 0x0d, 0x91, 0xc0, 0x03, 0x5f, 0xd6};
constexpr unsigned char kVariable[] = {0xff, 0xc3, 0x00, 0xd1, 0xfd, 0x7b, 0x01, 0xa9, 0xf4, 0x4f, 0x02, 0xa9, 0xfd, 0x43, 0x00, 0x91, 0x54, 0xd0, 0x3b, 0xd5, 0xf3, 0x03, 0x00, 0xaa, 0xe0, 0x03, 0x01, 0xaa, 0x88, 0x16, 0x40, 0xf9};
constexpr unsigned char kNumber[] = {0x00, 0x20, 0x00, 0x91, 0xc0, 0x03, 0x5f, 0xd6};
constexpr unsigned char kMesh[] = {0xf4, 0x4f, 0x05, 0xa9, 0xfd, 0x03, 0x00, 0x91, 0xff, 0x83, 0x0e, 0xd1, 0x5c, 0xd0, 0x3b, 0xd5, 0x88, 0x17, 0x40, 0xf9, 0xa8, 0x03, 0x1f, 0xf8, 0x08, 0x00, 0x40, 0x39, 0x48, 0xa0, 0x00, 0x34, 0x7b, 0xa2, 0x04, 0xf0};
constexpr unsigned char kScissor[] = {0xf4, 0x4f, 0x05, 0xa9, 0xfd, 0x03, 0x00, 0x91, 0x19, 0x00, 0x40, 0xf9, 0x1a, 0xfa, 0x82, 0x52, 0xf3, 0x03, 0x00, 0xaa, 0xba, 0x1b, 0xa0, 0x72, 0xf4, 0x03, 0x04, 0x2a, 0xf5, 0x03, 0x03, 0x2a};
constexpr unsigned char kScreen[] = {0x00, 0x14, 0x40, 0xf9, 0xc0, 0x03, 0x5f, 0xd6};
constexpr unsigned char kClip[] = {0x08, 0x00, 0x40, 0xf9, 0x49, 0x00, 0x40, 0x39, 0x00, 0x09, 0x40, 0xf9, 0x29, 0x03, 0x00, 0x34, 0x28, 0x08, 0x40, 0x79, 0x29, 0x0c, 0x40, 0x79, 0x42, 0x8c, 0x41, 0x2d, 0x00, 0x01, 0x22, 0x1e, 0x21, 0x01, 0x22, 0x1e, 0x40, 0x08, 0x20, 0x1e, 0x61, 0x08, 0x21, 0x1e, 0x03, 0x00, 0x38, 0x1e, 0x24, 0x00, 0x38, 0x1e, 0x7f, 0x00, 0x00, 0x71, 0x84, 0x18, 0x40, 0x7a, 0xa0, 0x02, 0x00, 0x54, 0x00, 0x01, 0x22, 0x1e, 0x21, 0x01, 0x22, 0x1e, 0x2a, 0x00, 0x40, 0x79, 0x42, 0x8c, 0x40, 0x2d, 0x2b, 0x04, 0x40, 0x79, 0x40, 0x08, 0x20, 0x1e, 0x61, 0x08, 0x21, 0x1e, 0x08, 0x00, 0x38, 0x1e, 0x29, 0x00, 0x38, 0x1e, 0x01, 0x01, 0x0a, 0x0b, 0x22, 0x01, 0x0b, 0x0b, 0x31, 0x3c, 0x03, 0x14};
constexpr Entry kEntries[] = {
    {0xb2d447c, 20, kActorPass, sizeof(kActorPass)},
    {0xa647524, 20, kCollectorInit, sizeof(kCollectorInit)},
    {0xa79f914, 20, kRender, sizeof(kRender)},
    {0x9808050, 20, kLocalPlayer, sizeof(kLocalPlayer)},
    {0xa965dbc, 0, kOptions, sizeof(kOptions)},
    {0xf591bec, 0, kVariables, sizeof(kVariables)},
    {0xf7aa69c, 0, kVariable, sizeof(kVariable)},
    {0xf772c68, 0, kNumber, sizeof(kNumber)},
    {0x9f0c25c, 20, kMesh, sizeof(kMesh)},
    {0x113c8ee8, 20, kScissor, sizeof(kScissor)},
    {0xa965dcc, 0, kScreen, sizeof(kScreen)},
    {0x112f9db8, 0, kClip, sizeof(kClip)},
    {0xac09d84, 0, kPerspective, sizeof(kPerspective)},
};

constexpr unsigned char kClipSource[] = {0x60, 0xf2, 0x41, 0x39, 0xad, 0xe3, 0x41, 0x39, 0x61, 0xa2, 0x01, 0x91, 0xae, 0xc3, 0x41, 0x39};
constexpr unsigned char kClipCopy[] = {0xa8, 0x63, 0x40, 0xf9, 0x7f, 0xa2, 0x07, 0x39, 0xa9, 0x43, 0x43, 0x39, 0x7f, 0xe2, 0x07, 0x39, 0xaa, 0x53, 0x41, 0x79, 0x0b, 0x41, 0x40, 0x39, 0x6b, 0x01, 0x00, 0x34, 0x00, 0x01, 0x40, 0xbd, 0x60, 0xea, 0x01, 0xbd, 0x00, 0x05, 0x40, 0xbd, 0x60, 0xee, 0x01, 0xbd, 0x00, 0x09, 0x40, 0xbd, 0x60, 0xf2, 0x01, 0xbd, 0x00, 0x0d, 0x40, 0xbd, 0x28, 0x00, 0x80, 0x52, 0x68, 0xe2, 0x07, 0x39};
constexpr unsigned char kMeshDeferred[] = {0xf7, 0x06, 0x00, 0xb4, 0xdb, 0x06, 0x00, 0x37, 0x80, 0x82, 0x00, 0x91, 0xfc, 0x4b, 0x00, 0xf9, 0xca, 0xcc, 0xbc, 0x95, 0x60, 0x17, 0x00, 0x36};
constexpr unsigned char kContextDispatcher[] = {0x0a, 0xa4, 0x43, 0xa9, 0x0a, 0x25, 0x00, 0xa9};
constexpr unsigned char kActorFlags[] = {0x28, 0x00, 0x80, 0x52, 0x29, 0x20, 0x80, 0x52, 0x08, 0xe8, 0x02, 0x39, 0x09, 0x70, 0x01, 0x79, 0xc0, 0x03, 0x5f, 0xd6};
constexpr unsigned char kDitherDistance[] = {0x00, 0xb4, 0x00, 0xbd, 0xc0, 0x03, 0x5f, 0xd6, 0x00, 0xb4, 0x40, 0xbd, 0xc0, 0x03, 0x5f, 0xd6};
constexpr unsigned char kDitherReject[] = {0x77, 0xf9, 0x09, 0x94, 0x10, 0x21, 0x20, 0x1e, 0xe0, 0x03, 0x13, 0xaa, 0xf3, 0x83, 0x95, 0x1a};
constexpr unsigned char kPassCollector[] = {0xe8, 0x03, 0x00, 0x2a, 0xe0, 0x83, 0x01, 0x91, 0x02, 0x01, 0x00, 0x12, 0xe1, 0x03, 0x17, 0xaa, 0x99, 0xca, 0xcd, 0x97};
constexpr Entry kLayoutGuards[] = {
    {0x9f0d388, 0, kClipSource, sizeof(kClipSource)},
    {0x10fdb08c, 0, kClipCopy, sizeof(kClipCopy)},
    {0x9f0c380, 0, kMeshDeferred, sizeof(kMeshDeferred)},
    {0xa9534a8, 0, kContextDispatcher, sizeof(kContextDispatcher)},
    {0xa965f34, 0, kActorFlags, sizeof(kActorFlags)},
    {0xa966088, 0, kDitherDistance, sizeof(kDitherDistance)},
    {0xa6e7ab4, 0, kDitherReject, sizeof(kDitherReject)},
    {0xb2d4ab0, 0, kPassCollector, sizeof(kPassCollector)},
};

}

bool installHooks() {
    if (active.load(std::memory_order_acquire)) return true;
    const auto base = elf::findModuleBase("libminecraftpe.so");
    if (!base || !supportedBuild(base)) return false;
    for (const auto& entry : kEntries) {
        const auto address = base + entry.rva + entry.offset;
        if (!readable(address, entry.length, true) || std::memcmp(reinterpret_cast<const void*>(address), entry.bytes, entry.length)) return false;
    }
    for (const auto& entry : kLayoutGuards) {
        const auto address = base + entry.rva + entry.offset;
        if (!readable(address, entry.length, true) || std::memcmp(reinterpret_cast<const void*>(address), entry.bytes, entry.length)) return false;
    }
    renderActor = reinterpret_cast<RenderFn>(base + kEntries[2].rva);
    getClientLocalPlayer = reinterpret_cast<ContextFn>(base + kEntries[3].rva);
    getOptions = reinterpret_cast<ContextFn>(base + kEntries[4].rva);
    getPerspective = reinterpret_cast<PerspectiveFn>(base + kEntries[12].rva);
    getVariables = reinterpret_cast<ContextFn>(base + kEntries[5].rva);
    getVariable = reinterpret_cast<VariableFn>(base + kEntries[6].rva);
    getNumber = reinterpret_cast<NumberFn>(base + kEntries[7].rva);
    setScissor = reinterpret_cast<ScissorFn>(base + kEntries[9].rva);
    getScreen = reinterpret_cast<ContextFn>(base + kEntries[10].rva);
    const std::array<void*, 4> detours{reinterpret_cast<void*>(&actorPassDetour), reinterpret_cast<void*>(&collectorInitDetour), reinterpret_cast<void*>(&meshDetour), reinterpret_cast<void*>(&clipDetour)};
    const std::array<std::atomic<void*>*, 4> originals{&originalPass, &originalCollectorInit, &originalMesh, &originalClip};
    constexpr std::array<unsigned, 4> entryIndices{0, 1, 8, 11};
    for (int i = 3; i >= 0; --i) {
        void* target = reinterpret_cast<void*>(base + kEntries[entryIndices[i]].rva);
        void* original = nullptr;
        if (pl::memory::hook(target, detours[i], &original, pl::memory::HookPriority::High) != 0 || !original) {
            removeHooks();
            return false;
        }
        originals[i]->store(original, std::memory_order_release);
        targets[i] = target;
    }
    expected.store(false, std::memory_order_relaxed);
    active.store(true, std::memory_order_release);
    return true;
}

void removeHooks() {
    active.store(false, std::memory_order_release);
    expected.store(false, std::memory_order_relaxed);
    const std::array<void*, 4> detours{reinterpret_cast<void*>(&actorPassDetour), reinterpret_cast<void*>(&collectorInitDetour), reinterpret_cast<void*>(&meshDetour), reinterpret_cast<void*>(&clipDetour)};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (targets[i]) (void)pl::memory::unhook(targets[i], detours[i]);
        targets[i] = nullptr;
    }
}

void setRendererReady(bool ready) { rendererReady.store(ready, std::memory_order_release); }
bool hooksActive() { return active.load(std::memory_order_acquire); }
bool casterExpected() { return wanted() && expected.load(std::memory_order_relaxed); }


}
