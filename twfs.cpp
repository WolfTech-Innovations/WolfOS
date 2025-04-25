// TWFS - Tiny Wolf File System (FUSE 2 version)
// Requirements: libfuse-dev, libzstd-dev
// Compile with: g++ -std=c++17 -D_FILE_OFFSET_BITS=64 -lfuse -lzstd -o twfs twfs.cpp

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <zstd.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

map<string, vector<char>> files; // path -> data
map<string, mode_t> modes;       // path -> mode

// Load compressed archive
bool load_archive(const string& archive_path) {
    ifstream file(archive_path, ios::binary);
    if (!file) return false;

    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        string name(name_len, '\0');
        file.read(&name[0], name_len);

        mode_t mode;
        file.read(reinterpret_cast<char*>(&mode), sizeof(mode));
        modes[name] = mode;

        uint32_t compressed_size;
        file.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size));
        vector<char> compressed(compressed_size);
        file.read(compressed.data(), compressed_size);

        unsigned long long decompressed_size = ZSTD_getFrameContentSize(compressed.data(), compressed_size);
        vector<char> decompressed(decompressed_size);
        ZSTD_decompress(decompressed.data(), decompressed_size, compressed.data(), compressed_size);

        files[name] = move(decompressed);
    }

    return true;
}

// Save compressed archive
bool save_archive(const string& archive_path) {
    ofstream file(archive_path, ios::binary);
    if (!file) return false;

    uint32_t count = files.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& pair : files) {
        const string& name = pair.first;
        const vector<char>& data = pair.second;
        uint32_t name_len = name.size();
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(name.c_str(), name_len);

        mode_t mode = modes[name];
        file.write(reinterpret_cast<const char*>(&mode), sizeof(mode));

        size_t bound = ZSTD_compressBound(data.size());
        vector<char> compressed(bound);
        size_t compressed_size = ZSTD_compress(compressed.data(), bound, data.data(), data.size(), 22);

        file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(uint32_t));
        file.write(compressed.data(), compressed_size);
    }

    return true;
}

// FUSE callbacks
static int twfs_getattr(const char* path, struct stat* stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (string(path) == "/" || modes.count(path)) {
        stbuf->st_mode = (string(path) == "/") ? S_IFDIR | 0755 : S_IFREG | modes[path];
        stbuf->st_nlink = 1;
        if (files.count(path)) stbuf->st_size = files[path].size();
        return 0;
    }
    return -ENOENT;
}

static int twfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    (void) offset;
    (void) fi;
    if (string(path) != "/") return -ENOENT;
    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);
    for (auto& pair : files) {
        string filename = pair.first.substr(1); // skip leading '/'
        filler(buf, filename.c_str(), nullptr, 0);
    }
    return 0;
}

static int twfs_open(const char* path, struct fuse_file_info* fi) {
    (void) fi;
    return files.count(path) ? 0 : -ENOENT;
}

static int twfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void) fi;
    if (!files.count(path)) return -ENOENT;
    const vector<char>& data = files[path];
    if (offset < (off_t)data.size()) {
        size_t len = min(size, data.size() - offset);
        memcpy(buf, data.data() + offset, len);
        return len;
    }
    return 0;
}

static struct fuse_operations twfs_oper = {
    .getattr    = twfs_getattr,
    .open       = twfs_open,
    .read       = twfs_read,
    .readdir    = twfs_readdir,
};

int main(int argc, char* argv[]) {
    if (argc >= 3 && string(argv[1]) == "mount") {
        string archive = argv[2];
        if (!load_archive(archive)) {
            cerr << "Failed to load archive: " << archive << endl;
            return 1;
        }
        return fuse_main(argc - 2, argv + 2, &twfs_oper, nullptr);

    } else if (argc >= 4 && string(argv[1]) == "pack") {
        string archive_path = argv[2];
        for (int i = 3; i < argc; ++i) {
            string path = argv[i];
            ifstream f(path, ios::binary);
            if (!f) {
                cerr << "Failed to open file: " << path << endl;
                continue;
            }
            vector<char> data((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
            string internal_path = "/" + path;
            files[internal_path] = data;
            modes[internal_path] = 0644;
        }
        if (!save_archive(archive_path)) {
            cerr << "Failed to save archive to: " << archive_path << endl;
            return 1;
        }
        cout << "Archive created: " << archive_path << endl;
        return 0;
    } else {
        cerr << "Usage:\n  " << argv[0] << " mount <archive.twfs> <mountpoint>\n  " << argv[0] << " pack <archive.twfs> <file1> <file2> ...\n";
        return 1;
    }
}
