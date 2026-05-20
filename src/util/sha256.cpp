#include "util/sha256.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace credis::util {

auto sha256(std::string_view input) -> std::string {
    static constexpr std::array<uint32_t, 64> K
        = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
           0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
           0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
           0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
           0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
           0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
           0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
           0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    auto rr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = input.size() * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>(bit_len >> (i * 8)));
    }

    uint32_t h0 = 0x6a09e667;
    uint32_t h1 = 0xbb67ae85;
    uint32_t h2 = 0x3c6ef372;
    uint32_t h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f;
    uint32_t h5 = 0x9b05688c;
    uint32_t h6 = 0x1f83d9ab;
    uint32_t h7 = 0x5be0cd19;

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        std::array<uint32_t, 64> w{};
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[offset + i * 4]) << 24)
                   | (static_cast<uint32_t>(msg[offset + i * 4 + 1]) << 16)
                   | (static_cast<uint32_t>(msg[offset + i * 4 + 2]) << 8)
                   | static_cast<uint32_t>(msg[offset + i * 4 + 3]);
        }
        for (size_t i = 16; i < 64; ++i) {
            auto s0 = rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            auto s1 = rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        auto a = h0;
        auto b = h1;
        auto c = h2;
        auto d = h3;
        auto e = h4;
        auto f = h5;
        auto g = h6;
        auto h = h7;
        for (size_t i = 0; i < 64; ++i) {
            auto S1 = rr(e, 6) ^ rr(e, 11) ^ rr(e, 25);
            auto ch = (e & f) ^ (~e & g);
            auto temp1 = h + S1 + ch + K[i] + w[i];
            auto S0 = rr(a, 2) ^ rr(a, 13) ^ rr(a, 22);
            auto maj = (a & b) ^ (a & c) ^ (b & c);
            auto temp2 = S0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += h;
    }

    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (uint32_t val : {h0, h1, h2, h3, h4, h5, h6, h7}) {
        for (int i = 24; i >= 0; i -= 8) {
            result += hex[(val >> (i + 4)) & 0x0F];
            result += hex[(val >> i) & 0x0F];
        }
    }
    return result;
}

} // namespace credis::util
