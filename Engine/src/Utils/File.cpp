#include "File.h"

#include <sstream>

#include "Debug/Log.h"

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

std::filesystem::file_time_type File::GetLastWriteTime(const std::filesystem::path& path)
{
    return std::filesystem::last_write_time(path);
}

std::filesystem::file_time_type File::GetLastWriteTime() const
{
    return GetLastWriteTime(m_path);
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
    if (size > 0)
    {
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

    while (std::getline(file, line))
    {
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
    if (!ec && sz > 0) 
        out.reserve(static_cast<size_t>(sz));

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

bool File::WriteAllBytes(const std::filesystem::path& path, const std::vector<uint8_t>& in)
{
    File file(path);
    return file.WriteAllBytes(in);
}

bool File::WriteAllLines(const std::filesystem::path& path, const std::vector<std::string>& in)
{
    File file(path);
    return file.WriteAllLines(in);
}

bool File::WriteAllText(const std::filesystem::path& path, const std::string& in)
{
    File file(path);
    return file.WriteAllText(in);
}

bool File::WriteAllBytes(const std::vector<uint8_t>& out) const
{
    std::ofstream file(m_path, std::ios::binary);
    if (!file.is_open())
    {
        PrintError("Failed to open file for writing: %s", m_path.string().c_str());
        return false;
    }

    file.write(reinterpret_cast<const char*>(out.data()), out.size());
    return true;
}

bool File::WriteAllLines(const std::vector<std::string>& out) const
{
    std::ofstream file(m_path);
    if (!file.is_open())
    {
        PrintError("Failed to open file for writing: %s", m_path.string().c_str());
        return false;
    }

    for (const auto& line : out)
    {
        file << line << '\n';
    }

    return true;
}

bool File::WriteAllText(const std::string& out) const
{
    std::ofstream file(m_path, std::ios::binary);
    if (!file.is_open())
    {
        PrintError("Failed to open file for writing: %s", m_path.string().c_str());
        return false;
    }

    file << out;
    return true;
}
