#include "core/base64.h"

#include <array>
#include <fstream>
#include <iterator>
#include <vector>

namespace oop {
namespace {

constexpr std::array<char, 64> alphabet{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

std::string encode(const std::vector<unsigned char>& bytes) {
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::size_t remaining = bytes.size() - i;
        const unsigned int first = bytes[i];
        const unsigned int second = remaining > 1 ? bytes[i + 1] : 0;
        const unsigned int third = remaining > 2 ? bytes[i + 2] : 0;
        out.push_back(alphabet[(first >> 2) & 0x3F]);
        out.push_back(alphabet[((first & 0x03) << 4) | (second >> 4)]);
        out.push_back(remaining > 1 ? alphabet[((second & 0x0F) << 2) | (third >> 6)] : '=');
        out.push_back(remaining > 2 ? alphabet[third & 0x3F] : '=');
    }
    return out;
}

}  // namespace

Result<std::string> read_file_base64(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fail<std::string>("Cannot open image file: " + path.string());
    }
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty()) {
        return fail<std::string>("Image file is empty: " + path.string());
    }
    return encode(bytes);
}

}  // namespace oop
