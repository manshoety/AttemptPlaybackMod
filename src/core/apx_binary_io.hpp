#pragma once

// Using GeodeV6 file stuff for safety
#include <asp/fs/File.hpp>
#include <asp/fs/std.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace apxio {
enum class Origin { Begin, Current };
enum class Mode { Read, ReadAtEnd, Truncate, Append, CreateNew };

// Per-record and allocation limits
inline constexpr uint32_t kMaxChunkBytes = sizeof(void*) == 4 ? 32u * 1024u * 1024u : 256u * 1024u * 1024u;
inline constexpr uint64_t kMaxDecodedFrameBytes = sizeof(void*) == 4 ? 64ull * 1024u * 1024u : 256ull * 1024u * 1024u;
inline constexpr uint64_t kMaxBulkFrameBytes = sizeof(void*) == 4 ? 96ull * 1024u * 1024u : 512ull * 1024u * 1024u;
inline constexpr uint32_t kMaxCatalogPayload = sizeof(void*) == 4 ? 32u * 1024u * 1024u : 128u * 1024u * 1024u;
inline constexpr uint32_t kMaxCatalogEntries = sizeof(void*) == 4 ? 250'000u : 1'000'000u;

class Reader {
    asp::fs::File m_file;
    std::span<const uint8_t> m_bytes;

    uint64_t m_size = 0, m_pos = 0;
    bool m_memory = false, m_ok = false, m_eof = false;
    std::string m_error;
public:
    Reader() = default;

    explicit Reader(std::span<const uint8_t> bytes)
      : m_bytes(bytes), m_size(bytes.size()), m_memory(true), m_ok(true) {}

    explicit Reader(std::filesystem::path const& path, Mode mode = Mode::Read) { open(path, mode); }

    void open(std::filesystem::path const& path, Mode mode = Mode::Read) {
        m_file = asp::fs::File{};
        m_memory = false;
        m_ok = false;
        m_eof = false;
        m_pos = m_size = 0;
        m_error.clear();

        if (path.empty()) {
            m_error = "Empty file path.";
            return;
        }
        auto opened = asp::fs::File::open(asp::fs::Path::fromStdPath(path));
        if (!opened) {
            m_error = opened.unwrapErr().toString();
            return;
        }

        m_file = std::move(opened.unwrap());
        auto size = m_file.size();
        if (!size || size.unwrap() > uint64_t(INT64_MAX)) {
            m_error = !size ? size.unwrapErr().toString() : "File is too large.";
            m_file.close();
            return;
        }

        m_size = size.unwrap();
        m_ok = true;

        if (mode == Mode::ReadAtEnd) seekg(static_cast<int64_t>(m_size));
    }
    
    bool is_open() const { return m_memory || m_file.valid(); }
    explicit operator bool() const { return m_ok; }
    bool good() const { return m_ok; }
    bool eof() const { return m_eof; }
    std::string const& error() const { return m_error; }
    void clear() {
        m_ok = is_open();
        m_eof = false;
        m_error.clear();
    }
    int64_t tellg() const { return m_ok ? static_cast<int64_t>(m_pos) : -1; }
    uint64_t remaining() const { return m_ok ? m_size - m_pos : 0; }

    void seekg(int64_t offset, Origin origin = Origin::Begin) {
        if (!m_ok || offset < 0) {
            m_ok = false;
            return;
        }
        uint64_t base = origin == Origin::Current ? m_pos : 0;
        if (uint64_t(offset) > m_size - base) {
            m_ok = false;
            return;
        }
        auto pos = base + uint64_t(offset);
        if (!m_memory && !m_file.seek(pos)) {
            m_ok = false;
            return;
        }
        m_pos = pos;
        m_eof = false;
    }

    void read(char* dst, size_t count) {
        if (!m_ok || count == 0) return;
        if (count > remaining()) {
            m_eof = remaining() == 0;
            m_error = "Unexpected end of file.";
            m_ok = false;
            return;
        }
        if (m_memory) std::memcpy(dst, m_bytes.data() + m_pos, count);
        else {
            auto result = m_file.readExact({reinterpret_cast<uint8_t*>(dst), count});
            if (!result) {
                m_error = result.unwrapErr().toString();
                m_ok = false;
                return;
            }
        }
        m_pos += count;
    }

    void close() {
        if (m_file.valid()) m_file.close();
        m_memory = false;
        m_ok = false;
    }
};

class Writer {
    asp::fs::File m_file;
    std::vector<uint8_t>* m_bytes = nullptr;
    bool m_ok = false;
    size_t m_limit = SIZE_MAX;
    std::string m_error;
public:
    explicit Writer(std::vector<uint8_t>& bytes, size_t limit = SIZE_MAX)
        : m_bytes(&bytes), m_ok(bytes.size() <= limit), m_limit(limit) {}
    explicit Writer(std::filesystem::path const& path, Mode mode = Mode::Truncate) {
        if (path.empty()) {
            m_error = "Empty file path.";
            return;
        }
        auto native = asp::fs::Path::fromStdPath(path);
        if (mode == Mode::Append) {
            // Single writer on game thread
            asp::fs::OpenOptions opts{};
            opts.read = false;
            opts.write = true;
            auto opened = asp::fs::File::open(native, opts);

            if (!opened) {
                m_error = opened.unwrapErr().toString();
                return;
            }

            m_file = std::move(opened.unwrap());
            auto size = m_file.size();
            if (!size) {
                m_error = size.unwrapErr().toString();
                return;
            }
            auto seek = m_file.seek(size.unwrap());
            if (!seek) {
                m_error = seek.unwrapErr().toString();
                return;
            }
            m_ok = true;
        } else {
            auto opened = mode == Mode::CreateNew ? asp::fs::File::createNew(native) : asp::fs::File::create(native);
            
            if (!opened) {
                m_error = opened.unwrapErr().toString();
                return;
            }

            m_file = std::move(opened.unwrap());
            m_ok = true;
        }
    }
    explicit operator bool() const { return m_ok; }
    bool good() const { return m_ok; }
    std::string const& error() const { return m_error; }

    void write(char const* src, size_t count) {
        if (!m_ok || !count) return;

        auto data = reinterpret_cast<uint8_t const*>(src);

        if (m_bytes) {
            if (count > m_limit - m_bytes->size() || count > m_bytes->max_size() - m_bytes->size()) {
                m_ok = false;
                return;
            }
            m_bytes->insert(m_bytes->end(), data, data + count);

        } else {
            auto result = m_file.writeAll({data, count});
            if (!result) {
                m_error = result.unwrapErr().toString();
                m_ok = false;
            }
        }
    }

    uint64_t position() {
        if (!m_ok || !m_file.valid()) return UINT64_MAX;
        auto pos = m_file.tell();
        if (!pos) {
            m_error = pos.unwrapErr().toString();
            return UINT64_MAX;
        }
        return pos.unwrap();
    }

    bool rollback(uint64_t position) {
        if (!m_file.valid() || position == UINT64_MAX) return false;
        // Caller stops writing after rollback even on success
        m_ok = false;
        return m_file.setLength(position).isOk();
    }
    
    void close() { if (m_file.valid()) m_file.close(); m_ok = false; }
};
}