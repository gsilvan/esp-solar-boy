#pragma once
#include "Arduino.h"
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <cstring>

class File {
    std::string _root, _path;
    bool _valid = false, _directory = false;
    size_t _size = 0, _next = 0;
    std::vector<std::string> _entries;
public:
    File() = default;
    File(const std::string &root, const std::string &path) : _root(root), _path(path) {
        struct stat info;
        const auto hostPath = _root + _path;
        _valid = stat(hostPath.c_str(), &info) == 0;
        if (!_valid) return;
        _directory = S_ISDIR(info.st_mode);
        _size = info.st_size;
        if (_directory) {
            DIR *dir = opendir(hostPath.c_str());
            if (!dir) { _valid = false; return; }
            while (auto *entry = readdir(dir)) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
                    _entries.push_back(_path + "/" + entry->d_name);
            }
            closedir(dir);
        }
    }
    operator bool() const { return _valid; }
    bool isDirectory() const { return _directory; }
    const char *path() const { return _path.c_str(); }
    size_t size() const { return _size; }
    File openNextFile() { return _next < _entries.size() ? File(_root, _entries[_next++]) : File(); }
    String readString() const {
        std::ifstream stream(_root + _path);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    void close() { _valid = false; }
};
class Dir {
    File directory, current;
public:
    explicit Dir(File file) : directory(file) {}
    bool next() { current = directory.openNextFile(); return bool(current); }
    bool isDirectory() const { return current.isDirectory(); }
    String fileName() const {
        const std::string path(current.path());
        return path.substr(path.find_last_of('/') + 1);
    }
};
struct FakeLittleFS {
    std::string root = "data";
    File open(const char *path, const char * = "r") { return File(root, path); }
    Dir openDir(const char *path) { return Dir(File(root, path)); }
};
extern FakeLittleFS LittleFS;
