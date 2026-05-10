#include "bytecode_io.hpp"

#include <bit>
#include <cassert>
#include <cstring>
#include <format>
#include <stdexcept>

// ── Magic ─────────────────────────────────────────────────────────────────────
static constexpr uint8_t kMagic[4] = {'D', 'B', 'C', 0x01};

// ─── Low-level write helpers ──────────────────────────────────────────────────

static void write_bytes(std::ostream& os, const void* data, std::size_t n) {
    os.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
    if (!os)
        throw std::runtime_error("write error");
}

// Write an integer/float in little-endian byte order.
template <class T>
static void write_le(std::ostream& os, T v) {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
    char buf[sizeof(T)];
    std::memcpy(buf, &v, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) {
        for (std::size_t i = 0; i < sizeof(T) / 2; ++i)
            std::swap(buf[i], buf[sizeof(T) - 1 - i]);
    }
    write_bytes(os, buf, sizeof(T));
}

static void write_str(std::ostream& os, const std::string& s) {
    if (s.size() > UINT32_MAX)
        throw std::runtime_error("string too long");
    write_le<uint32_t>(os, static_cast<uint32_t>(s.size()));
    if (!s.empty()) write_bytes(os, s.data(), s.size());
}

static void write_name(std::ostream& os, const std::string& s) {
    if (s.size() > UINT16_MAX)
        throw std::runtime_error("name too long");
    write_le<uint16_t>(os, static_cast<uint16_t>(s.size()));
    if (!s.empty()) write_bytes(os, s.data(), s.size());
}

// ─── Low-level read helpers ───────────────────────────────────────────────────

static void read_bytes(std::istream& is, void* data, std::size_t n) {
    is.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(n));
    if (static_cast<std::size_t>(is.gcount()) != n)
        throw std::runtime_error("unexpected end of .dbc file");
}

template <class T>
static T read_le(std::istream& is) {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
    char buf[sizeof(T)];
    read_bytes(is, buf, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) {
        for (std::size_t i = 0; i < sizeof(T) / 2; ++i)
            std::swap(buf[i], buf[sizeof(T) - 1 - i]);
    }
    T v;
    std::memcpy(&v, buf, sizeof(T));
    return v;
}

static std::string read_str(std::istream& is) {
    uint32_t len = read_le<uint32_t>(is);
    std::string s(len, '\0');
    if (len > 0) read_bytes(is, s.data(), len);
    return s;
}

static std::string read_name(std::istream& is) {
    uint16_t len = read_le<uint16_t>(is);
    std::string s(len, '\0');
    if (len > 0) read_bytes(is, s.data(), len);
    return s;
}

// ─── Proto serializer ─────────────────────────────────────────────────────────

static void write_proto(const Proto& p, std::ostream& os) {
    write_name(os, p.name);
    write_le<int32_t>(os, static_cast<int32_t>(p.params));
    write_le<int32_t>(os, static_cast<int32_t>(p.regs));
    write_le<int32_t>(os, static_cast<int32_t>(p.cells));

    // Constants
    write_le<uint32_t>(os, static_cast<uint32_t>(p.consts.size()));
    for (const auto& cv : p.consts) {
        std::visit(
            [&os]<class T>(const T& v) {
                if constexpr (std::is_same_v<T, std::monostate>) {
                    write_le<uint8_t>(os, uint8_t{0});
                } else if constexpr (std::is_same_v<T, long long>) {
                    write_le<uint8_t>(os, uint8_t{1});
                    write_le<int64_t>(os, static_cast<int64_t>(v));
                } else if constexpr (std::is_same_v<T, double>) {
                    write_le<uint8_t>(os, uint8_t{2});
                    write_le<double>(os, v);
                } else {
                    write_le<uint8_t>(os, uint8_t{3});
                    write_str(os, v);
                }
            },
            cv);
    }

    // Upvalues
    write_le<uint32_t>(os, static_cast<uint32_t>(p.upvals.size()));
    for (const auto& u : p.upvals) {
        write_name(os, u.name);
        write_le<uint8_t>(os, u.is_local ? uint8_t{1} : uint8_t{0});
        write_le<int32_t>(os, u.idx);
    }

    // Instructions: op(u8) + a,b,c,d(i32) = 17 bytes each
    write_le<uint32_t>(os, static_cast<uint32_t>(p.code.size()));
    for (const auto& ins : p.code) {
        write_le<uint8_t>(os, static_cast<uint8_t>(ins.op));
        write_le<int32_t>(os, ins.a);
        write_le<int32_t>(os, ins.b);
        write_le<int32_t>(os, ins.c);
        write_le<int32_t>(os, ins.d);
    }

    // Nested protos
    write_le<uint32_t>(os, static_cast<uint32_t>(p.protos.size()));
    for (const auto& sub : p.protos)
        write_proto(*sub, os);
}

// ─── Proto deserializer ───────────────────────────────────────────────────────

static std::shared_ptr<Proto> read_proto(std::istream& is) {
    auto p    = std::make_shared<Proto>();
    p->name   = read_name(is);
    p->params = static_cast<int>(read_le<int32_t>(is));
    p->regs   = static_cast<int>(read_le<int32_t>(is));
    p->cells  = static_cast<int>(read_le<int32_t>(is));

    // Constants
    uint32_t nc = read_le<uint32_t>(is);
    p->consts.reserve(nc);
    for (uint32_t i = 0; i < nc; ++i) {
        uint8_t tag = read_le<uint8_t>(is);
        switch (tag) {
        case 0: p->consts.emplace_back(std::monostate{}); break;
        case 1: p->consts.emplace_back(static_cast<long long>(read_le<int64_t>(is))); break;
        case 2: p->consts.emplace_back(read_le<double>(is)); break;
        case 3: p->consts.emplace_back(read_str(is)); break;
        default:
            throw std::runtime_error(
                std::format("unknown const tag {:#x}", static_cast<unsigned>(tag)));
        }
    }

    // Upvalues
    uint32_t nu = read_le<uint32_t>(is);
    p->upvals.reserve(nu);
    for (uint32_t i = 0; i < nu; ++i) {
        UpvalDesc u;
        u.name     = read_name(is);
        u.is_local = read_le<uint8_t>(is) != 0;
        u.idx      = read_le<int32_t>(is);
        p->upvals.push_back(std::move(u));
    }

    // Instructions
    uint32_t ni = read_le<uint32_t>(is);
    p->code.reserve(ni);
    for (uint32_t i = 0; i < ni; ++i) {
        Instr ins;
        ins.op = static_cast<Opc>(read_le<uint8_t>(is));
        ins.a  = read_le<int32_t>(is);
        ins.b  = read_le<int32_t>(is);
        ins.c  = read_le<int32_t>(is);
        ins.d  = read_le<int32_t>(is);
        p->code.push_back(ins);
    }

    // Nested protos
    uint32_t np = read_le<uint32_t>(is);
    p->protos.reserve(np);
    for (uint32_t i = 0; i < np; ++i)
        p->protos.push_back(read_proto(is));

    return p;
}

// ─── Public API ───────────────────────────────────────────────────────────────

void write_module(const Module& m, std::ostream& os) {
    write_bytes(os, kMagic, sizeof(kMagic));
    write_proto(*m.main, os);
}

Module read_module(std::istream& is) {
    uint8_t magic[4];
    read_bytes(is, magic, 4);
    if (magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
        magic[2] != kMagic[2] || magic[3] != kMagic[3]) {
        throw std::runtime_error(
            "not a .dbc file (bad magic); expected DBC\\x01");
    }
    Module m;
    m.main = read_proto(is);
    return m;
}
