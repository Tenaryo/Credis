#include "rdb_parser.hpp"

#include <cstdint>
#include <cstring>
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

    [[nodiscard]] auto has_more() const -> bool {
        return pos_ < size_;
    }

    [[nodiscard]] auto peek() const -> uint8_t {
        return data_[pos_];
    }

    auto read_byte() -> uint8_t {
        if (pos_ >= size_) {
            return 0;
        }
        return data_[pos_++];
    }

    auto read_bytes(size_t n) -> std::vector<uint8_t> {
        if (pos_ + n > size_) {
            n = size_ - pos_;
        }
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
        if (!has_more()) {
            return {};
        }
        auto first = data_[pos_];
        auto hi2 = (first & 0xC0) >> 6;

        if (hi2 == 3) {
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
};

} // namespace

auto parse_rdb(const std::vector<uint8_t>& data) -> std::unordered_map<std::string, RdbEntry> {
    Reader reader(data.data(), data.size());

    if (data.size() < 9 || std::memcmp(data.data(), "REDIS", 5) != 0) {
        return {};
    }

    for (int i = 0; i < 9; ++i) {
        reader.read_byte();
    }

    std::unordered_map<std::string, RdbEntry> result;

    while (reader.has_more()) {
        auto op = reader.read_byte();

        if (op == 0xFA) {
            reader.read_string();
            reader.read_string();
        } else if (op == 0xFE) {
            reader.read_length();

            if (reader.has_more() && reader.peek() == 0xFB) {
                reader.read_byte();
                reader.read_length();
                reader.read_length();
            }

            while (reader.has_more()) {
                auto peek = reader.peek();
                if (peek == 0xFF || peek == 0xFE || peek == 0xFA) {
                    break;
                }

                std::optional<uint64_t> expire_ms;

                if (peek == 0xFD) {
                    reader.read_byte();
                    expire_ms = static_cast<uint64_t>(reader.read_le32()) * 1000;
                } else if (peek == 0xFC) {
                    reader.read_byte();
                    expire_ms = reader.read_le64();
                }

                // TODO: only supports String (0x00); need List, Set, ZSet, Stream encodings
                reader.read_byte();
                auto key = reader.read_string();
                auto value = reader.read_string();

                result.emplace(std::move(key), RdbEntry{credis::store::String(std::move(value)), expire_ms});
            }
        } else if (op == 0xFF) {
            break;
        }
    }

    return result;
}

auto load_rdb_file(const std::string& path) -> std::unordered_map<std::string, RdbEntry> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    auto pos = file.tellg();
    if (pos < 0) {
        return {};
    }
    auto size = static_cast<size_t>(pos);
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

    return parse_rdb(data);
}

} // namespace credis::rdb
