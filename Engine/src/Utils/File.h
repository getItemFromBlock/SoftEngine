#pragma once
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>

class File
{
public:
    File(std::filesystem::path path) : m_path(std::move(path)) { }
    File(const std::string& path) : File(std::filesystem::path(path)) {}
    
    std::filesystem::path GetPath() const { return m_path; }
    
    static bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out);
    static bool ReadAllLines(const std::filesystem::path& path, std::vector<std::string>& out);
    static bool ReadAllText(const std::filesystem::path& path, std::string& out);
    static bool Exist(const std::filesystem::path& path);
    
    static std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& path);
    std::filesystem::file_time_type GetLastWriteTime() const;

    bool ReadAllBytes(std::vector<uint8_t>& out) const;
    bool ReadAllLines(std::vector<std::string>& out) const;
    bool ReadAllText(std::string& out) const;
    bool Exist() const;
    
    static bool WriteAllBytes(const std::filesystem::path& path, const std::vector<uint8_t>& in);
    static bool WriteAllLines(const std::filesystem::path& path, const std::vector<std::string>& in);
    static bool WriteAllText(const std::filesystem::path& path, const std::string& in);
    
    bool WriteAllBytes(const std::vector<uint8_t>& out) const;
    bool WriteAllLines(const std::vector<std::string>& out) const;
    bool WriteAllText(const std::string& out) const;
private:
    std::filesystem::path m_path;
};
