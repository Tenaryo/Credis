#include "rdb_parser.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace credis::rdb {

namespace {

class Reader {
    const uint8_t* data_;
    size_t size_;
    size_t pos_{0};

  public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {
    }

    auto read_byte() -> uint8_t {
        return data_[pos_++];
    }

    auto read_bytes(size_t n) -> std::vector<uint8_t> {
        const auto* start = data_ + pos_;
        pos_ += n;
        return {start, start + n};
    }

    auto read_be16() -> uint16_t {
        uint16_t v
            = static_cast<uint16_t>((static_cast<uint16_t>(data_[pos_]) << 8) | static_cast<uint16_t>(data_[pos_ + 1]));
        pos_ += 2;
        return v;
    }

    auto read_be32() -> uint32_t {
        auto v = (uint32_t(data_[pos_]) << 24) | (uint32_t(data_[pos_ + 1]) << 16) | (uint32_t(data_[pos_ + 2]) << 8)
                 | uint32_t(data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }

    auto read_le16() -> uint16_t {
        uint16_t v
            = static_cast<uint16_t>(static_cast<uint16_t>(data_[pos_]) | (static_cast<uint16_t>(data_[pos_ + 1]) << 8));
        pos_ += 2;
        return v;
    }

    auto read_le32() -> uint32_t {
        auto v = uint32_t(data_[pos_]) | (uint32_t(data_[pos_ + 1]) << 8) | (uint32_t(data_[pos_ + 2]) << 16)
                 | (uint32_t(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }

    auto read_le64() -> uint64_t {
        uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8) | static_cast<uint64_t>(data_[static_cast<size_t>(pos_ + i)]);
        }
        pos_ += 8;
        return v;
    }

    auto read_length() -> size_t {
        auto first = read_byte();
        auto hi2 = (first & 0xC0) >> 6;
        switch (hi2) {
        case 0:
            return first & 0x3F;
        case 1:
            return (size_t(first & 0x3F) << 8) | read_byte();
        case 2:
            (void)read_byte();
            return read_be32();
        default:
            return first & 0x3F;
        }
    }

    auto read_string() -> std::string {
        auto first = data_[pos_];
        auto hi2 = (first & 0xC0) >> 6;

        if (hi2 == 3) [[unlikely]] {
            pos_++;
            switch (first & 0x3F) {
            case 0:
                return std::to_string(read_byte());
            case 1:
                return std::to_string(read_le16());
            case 2:
                return std::to_string(read_le32());
            default:
                return {};
            }
        }

        auto len = read_length();
        auto bytes = read_bytes(len);
        return {bytes.begin(), bytes.end()};
    }

    [[nodiscard]] auto has_more() const -> bool {
        return pos_ < size_;
    }
    [[nodiscard]] auto pos() const -> size_t {
        return pos_;
    }
    [[nodiscard]] auto peek() const -> uint8_t {
        return data_[pos_];
    }
};

} // namespace

auto parse_rdb(const std::vector<uint8_t>& data) -> std::unordered_map<std::string, RdbEntry> {
    Reader reader(data.data(), data.size());
    std::unordered_map<std::string, RdbEntry> result;

    for (int i = 0; i < 9; ++i) {
        reader.read_byte();
    }

    while (reader.has_more()) [[likely]] {
        auto op = reader.read_byte();

        if (op == 0xFA) [[likely]] {
            reader.read_string();
            reader.read_string();
        } else if (op == 0xFE) [[unlikely]] {
            reader.read_length();

            if (reader.has_more() && reader.peek() == 0xFB) [[unlikely]] {
                reader.read_byte();
                reader.read_length();
                reader.read_length();
            }

            while (reader.has_more()) [[likely]] {
                auto peek = reader.peek();
                if (peek == 0xFF || peek == 0xFE || peek == 0xFA) [[unlikely]] {
                    break;
                }

                std::optional<uint64_t> expire_ms;

                if (peek == 0xFD) [[likely]] {
                    reader.read_byte();
                    expire_ms = static_cast<uint64_t>(reader.read_le32()) * 1000;
                } else if (peek == 0xFC) [[unlikely]] {
                    reader.read_byte();
                    expire_ms = reader.read_le64();
                }

                // TODO: only supports String (0x00); need List, Set, ZSet, Stream encodings
                reader.read_byte();
                auto key = reader.read_string();
                auto value = reader.read_string();

                result.emplace(std::move(key), RdbEntry{credis::store::String(std::move(value)), expire_ms});
            }
        } else if (op == 0xFF) [[unlikely]] {
            break;
        }
    }

    return result;
}

auto load_rdb_file(const std::string& path) -> std::unordered_map<std::string, RdbEntry> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) [[unlikely]] {
        return {};
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    return parse_rdb(data);
}

} // namespace credis::rdb
