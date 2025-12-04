#include "File.h"

#include <sstream>

bool File::ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out)
{
    File file(path);
    return file.ReadAllBytes(out);
}

bool File::ReadAllLines(const std::filesystem::path& path, std::vector<std::string>& out)
{
    File file(path);
    return file.ReadAllLines(out);
}

bool File::ReadAllText(const std::filesystem::path& path, std::string& out)
{
    File file(path);
    return file.ReadAllText(out);
}

bool File::Exist(const std::filesystem::path& path)
{
    return std::filesystem::exists(path);
}

bool File::ReadAllBytes(std::vector<uint8_t>& out) const
{
    std::ifstream file(m_path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    std::streamoff size = file.tellg();
    if (size < 0)
        return false;

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0) {
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())))
            return false;
    }

    out.swap(buffer);
    return true;
}

bool File::ReadAllLines(std::vector<std::string>& out) const
{
    std::ifstream file(m_path);
    if (!file.is_open())
        return false;

    std::string line;
    
    while (std::getline(file, line)) {
        out.push_back(line);
    }

    if (!file.eof() && file.fail())
        return false;

    return true;
}

bool File::ReadAllText(std::string& out) const
{
    std::ifstream file(m_path, std::ios::binary);
    if (!file.is_open())
        return false;

    std::error_code ec;
    auto sz = std::filesystem::file_size(m_path, ec);
    if (!ec && sz > 0) out.reserve(static_cast<size_t>(sz));

    std::ostringstream ss;
    ss << file.rdbuf();
    if (!file && !file.eof())
        return false;

    out = ss.str();
    return true;
}

bool File::Exist() const
{
    return std::filesystem::exists(m_path);
}
