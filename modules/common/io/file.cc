// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: file

#include <glob.h>
#include "glog/logging.h"
#include "common/io/file.h"

namespace crdc {
namespace airi {
namespace util {

bool set_proto_to_ascii_file(const google::protobuf::Message& message,
                             const int& file_descriptor) {
    using google::protobuf::TextFormat;
    using google::protobuf::io::FileOutputStream;
    using google::protobuf::io::ZeroCopyOutputStream;
    if (file_descriptor < 0) {
        LOG(ERROR) << "Invalie file descriptor";
        return false;
    }
    ZeroCopyOutputStream *output = new FileOutputStream(file_descriptor);
    bool success = TextFormat::Print(message, output);
    delete output;
    close(file_descriptor);
    return success;
}

bool set_proto_to_ascii_file(const google::protobuf::Message& message,
                             const std::string& file_name) {
    int fd = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd < 0) {
        LOG(ERROR) << "Unable to open file " << file_name << " to write.";
        return false;
    }
    return set_proto_to_ascii_file(message, fd);
}

bool get_proto_from_ascii_file(const std::string& file_name,
                               google::protobuf::Message *message) {
    using google::protobuf::TextFormat;
    using google::protobuf::io::FileInputStream;
    using google::protobuf::io::ZeroCopyInputStream;
    int file_descriptor = open(file_name.c_str(), O_RDONLY);
    if (file_descriptor < 0) {
        if (file_name[0] != '/') {
            const char *work_root = gflags::StringFromEnv("CRDC_WOEK_ROOT", "crdc/common");
            std::string proto_file_path = get_absolute_path(work_root, file_name);
            file_descriptor = open(proto_file_path.c_str(), O_RDONLY);
        }
    }

    if (file_descriptor < 0) {
        LOG(ERROR) << "Failed to open file " << file_name << " in text mode.";
        return false;
    }

    ZeroCopyInputStream *input = new FileInputStream(file_descriptor);
    bool success = TextFormat::Parse(input, message);
    if (!success) {
        LOG(ERROR) << "Failed to parse file " << file_name << " as text proto.";
    }
    delete input;
    close(file_descriptor);
    return success;
}

bool set_proto_to_binary_file(const google::protobuf::Message& message,
                             const std::string& file_name) {
    std::fstream output(file_name, std::ios::out | std::ios::trunc | std::ios::binary);
    return message.SerializeToOstream(&output);
}

bool get_proto_from_binary_file(const std::string& file_name,
                                google::protobuf::Message *message) {
    std::fstream input(file_name, std::ios::in | std::ios::binary);
    if (!input.good()) {
        if (file_name[0] != '/') {
            const char *work_root = gflags::StringFromEnv("CRDC_WOEK_ROOT", "crdc/common");
            std::string proto_file_path = get_absolute_path(work_root, file_name);
            input.open(file_name, std::ios::in | std::ios::binary);
        }
    }

    if (!input.good()) {
        LOG(ERROR) << "Failed to open file " << file_name << " in binary mode.";
        return false;
    }

    if (!message->ParseFromIstream(&input)) {
        LOG(ERROR) << "Failed to parse file " << file_name << " as binary proto.";
        return false;
    }
    return true;
}

bool get_proto_from_file(const std::string& file_name,
                         google::protobuf::Message *message) {
    // Try the binary parser first if it's much likely a binary proto.
    static const std::string bin_extension = ".bin";
    if (std::equal(bin_extension.rbegin(), bin_extension.rend(), file_name.rbegin())) {
        return get_proto_from_binary_file(file_name, message) ||
               get_proto_from_ascii_file(file_name, message);
    }

    return get_proto_from_ascii_file(file_name, message) ||
           get_proto_from_binary_file(file_name, message);
}

bool get_content(const std::string& file_name,
                 std::string *content) {
    std::ifstream fin(file_name);
    if (!fin) {
        return false;
    }

    std::stringstream str_stream;
    str_stream << fin.rdbuf();
    *content = str_stream.str();
    return true;
}

std::string get_absolute_path(const std::string& prefix, const std::string& relative_path) {
    if (relative_path.empty()) {
        return prefix;
    }
    // if prefix is empty or relative path is already absolute
    if (prefix.empty() || relative_path.front() == '/') {
        return relative_path;
    }

    if (prefix.back() == '/') {
        return prefix + relative_path;
    }

    return prefix + '/' + relative_path;
}

bool is_path_exists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

bool is_directory_exists(const std::string& directory_path) {
    struct stat info;
    return stat(directory_path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

std::vector<std::string> glob(const std::string& pattern) {
    glob_t globs = {};
    std::vector<std::string> results;
    if (glob(pattern.c_str(), GLOB_TILDE, nullptr, &globs) == 0) {
        for (auto i = 0; i < globs.gl_pathc; ++i) {
            results.emplace_back(globs.gl_pathv[i]);
        }
    }
    globfree(&globs);
    return results;
}

bool copy_file(const std::string& from, const std::string& to) {
    std::ifstream src(from, std::ios::binary);
    if (!src) {
        LOG(WARNING) << "Source path could not be normally opened: " << from;
#if TARGET_IOS
        return false;
#else
        std::string command = "cp -r " + from + " " + to;
        DLOG(INFO) << command;
        const int ret = std::system(command.c_str());
        if (ret == 0) {
            DLOG(INFO) << "Copy success, command returns " << ret;
            return true;
        } else {
            DLOG(INFO) << "Copy error, command returns" << ret;
            return false;
        }
#endif
    }

    std::ofstream dst(to, std::ios::binary);
    if (!dst) {
        LOG(ERROR) << "Target path is not writable: " << to;
        return false;
    }
    dst << src.rdbuf();
    return true;
}

bool copy_directory(const std::string& from, const std::string& to) {
    DIR *directory = opendir(from.c_str());
    if (directory == nullptr) {
        LOG(ERROR) << "Cannot open directory " << from;
        return false;
    }

    bool ret = true;
    if (ensure_directory(to)) {
        struct dirent *entry;
        while ((entry = readdir(directory)) != nullptr) {
            // skip direcotry_path/. and directory_path/..
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                continue;
            }
            const std::string sub_path_from = from + "/" + entry->d_name;
            const std::string sub_path_to = to + "/" + entry->d_name;
            if (entry->d_type == DT_DIR) {
                ret &= copy_directory(sub_path_from, sub_path_to);
            } else {
                ret &= copy_file(sub_path_from, sub_path_to);
            }
        }
    } else {
        LOG(ERROR) << "Cannot create target directory " << to;
        ret = false;
    }
    closedir(directory);
    return ret;
}

bool copy(const std::string& from, const std::string& to) {
    return is_directory_exists(from) ? copy_directory(from, to) : copy_file(from, to);
}

bool ensure_directory(const std::string& directory_path) {
    std::string path = directory_path;
    for (auto i = 1; i < directory_path.size(); ++i) {
        if (directory_path[i] == '/') {
            // whenever a '/' is encountered, create a temporary view from
            // the start of the path to the character right before this.
            path[i] = 0;
            if (mkdir(path.c_str(), S_IRWXU) != 0) {
                if (errno != EEXIST) {
                    return false;
                }
            }

            // Revert the temporary view back to the original.
            path[i] = '/';
        }
    }

    // Make the filanl (full) directory.
    if (mkdir(path.c_str(), S_IRWXU) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    return true;
}

std::string get_current_path() {
    char tmp[PATH_MAX];
    return getcwd(tmp, sizeof(tmp)) ? std::string(tmp) : std::string("");
}

bool create_dir(const std::string& directory_path) {
    int ret = mkdir(directory_path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret != 0) {
        LOG(WARNING) << "failed to create dir. [dir: " << directory_path
                     << "] [err: " << strerror(errno) << "]";
        return false;
    }
    return true;
}

std::vector<std::string> list_sub_paths(const std::string &directory_path,
                                        const unsigned char d_type) {
  std::vector<std::string> result;
  DIR *directory = opendir(directory_path.c_str());
  if (directory == nullptr) {
    LOG(ERROR) << "Cannot open directory " << directory_path;
    return result;
  }

  struct dirent *entry;
  while ((entry = readdir(directory)) != nullptr) {
    // Skip "." and "..".
    if (entry->d_type == d_type && strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0) {
      result.emplace_back(entry->d_name);
    }
  }
  closedir(directory);
  return result;
}

}  // namespace util
}  // namespace airi
}  // namespace crdc
