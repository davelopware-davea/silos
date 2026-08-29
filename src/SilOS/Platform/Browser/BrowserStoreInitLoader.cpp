#include "SilOS/Platform/Browser/BrowserStoreInitLoader.h"

#include "SilOS/Store/InMemoryStoreBackend.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace {
bool has_suffix(const char *text, const char *suffix) {
  const std::size_t text_length = std::strlen(text);
  const std::size_t suffix_length = std::strlen(suffix);
  return text_length >= suffix_length &&
         std::strcmp(text + text_length - suffix_length, suffix) == 0;
}

bool join_path(std::string &destination, std::string_view parent,
               std::string_view child) {
  try {
    destination.assign(parent);
    destination.push_back('/');
    destination.append(child);
    return true;
  } catch (const std::bad_alloc &) {
    return false;
  } catch (const std::length_error &) {
    return false;
  }
}

bool read_file(const char *path, std::string &content) {
  struct stat details {};
  if (stat(path, &details) != 0 || details.st_size < 0) return false;
  const std::uintmax_t file_size = static_cast<std::uintmax_t>(details.st_size);
  if (file_size > content.max_size()) return false;
  const std::size_t length = static_cast<std::size_t>(file_size);
  try {
    content.resize(length);
  } catch (const std::bad_alloc &) {
    return false;
  } catch (const std::length_error &) {
    return false;
  }
  std::FILE *file = std::fopen(path, "rb");
  if (file == nullptr) return false;
  const std::size_t bytes_read = std::fread(content.data(), 1, length, file);
  const bool read_complete = bytes_read == length && std::ferror(file) == 0;
  const bool closed = std::fclose(file) == 0;
  const bool succeeded = read_complete && closed &&
                         std::memchr(content.data(), '\0', length) == nullptr;
  return succeeded;
}

bool append_source_lines(const char *store_name, const std::string &content,
                         InMemoryStoreBackend &stores,
                         StoreInitLoadResult &result) {
  if (!stores.create_store(store_name)) return false;
  std::size_t line_start = 0;
  std::uint32_t id = 1;
  while (line_start < content.size()) {
    std::size_t line_end = line_start;
    while (line_end < content.size() && content[line_end] != '\n' &&
           content[line_end] != '\r') {
      ++line_end;
    }
    const std::size_t line_length = line_end - line_start;
    if (id == 0) return false;
    const std::string_view line(content.data() + line_start, line_length);
    const InMemoryStoreFieldInput field{"text", line};
    if (!stores.append_row(store_name, id++, 1, &field, 1)) return false;
    ++result.source_row_count;

    if (line_end == content.size()) break;
    if (content[line_end] == '\r' &&
        (line_end + 1 == content.size() || content[line_end + 1] != '\n')) {
      return false;  // Reject ambiguous legacy line endings deterministically.
    }
    line_start = line_end + (content[line_end] == '\r' ? 2 : 1);
  }
  return true;
}

enum class CsvRecordResult { Record, End, Malformed, NoMemory };

CsvRecordResult read_csv_record(const char *&cursor,
                                std::vector<std::string> &values) {
  if (*cursor == '\0') return CsvRecordResult::End;
  values.clear();
  try {
    for (;;) {
      std::string value;
      const bool quoted = *cursor == '"';
      if (quoted) ++cursor;

      bool closed_quote = !quoted;
      while (*cursor != '\0') {
        const char character = *cursor;
        if (quoted && character == '"') {
          if (cursor[1] == '"') {
            value.push_back('"');
            cursor += 2;
            continue;
          }
          ++cursor;
          closed_quote = true;
          break;
        }
        if (!quoted &&
            (character == ',' || character == '\n' || character == '\r')) {
          break;
        }
        if (!quoted && character == '"') return CsvRecordResult::Malformed;
        if (quoted && (character == '\n' || character == '\r')) {
          return CsvRecordResult::Malformed;
        }
        value.push_back(character);
        ++cursor;
      }
      if (!closed_quote) return CsvRecordResult::Malformed;
      values.push_back(std::move(value));

      if (*cursor == ',') {
        ++cursor;
        continue;
      }
      if (*cursor == '\r') {
        if (cursor[1] != '\n') return CsvRecordResult::Malformed;
        cursor += 2;
        return CsvRecordResult::Record;
      }
      if (*cursor == '\n') {
        ++cursor;
        return CsvRecordResult::Record;
      }
      if (*cursor == '\0') return CsvRecordResult::Record;
      return CsvRecordResult::Malformed;
    }
  } catch (const std::bad_alloc &) {
    return CsvRecordResult::NoMemory;
  } catch (const std::length_error &) {
    return CsvRecordResult::NoMemory;
  }
}

bool append_csv_rows(const char *store_name, const char content[],
                     InMemoryStoreBackend &stores, StoreInitLoadResult &result) {
  if (!stores.create_store(store_name)) return false;
  const char *cursor = content;
  std::vector<std::string> headers;
  if (read_csv_record(cursor, headers) != CsvRecordResult::Record ||
      headers.empty()) {
    return false;
  }
  for (std::size_t index = 0; index < headers.size(); ++index) {
    if (headers[index].empty()) return false;
    for (std::size_t earlier = 0; earlier < index; ++earlier) {
      if (headers[earlier] == headers[index]) return false;
    }
  }

  std::uint32_t id = 1;
  for (;;) {
    std::vector<std::string> values;
    const CsvRecordResult parsed = read_csv_record(cursor, values);
    if (parsed == CsvRecordResult::End) return true;
    if (parsed != CsvRecordResult::Record || values.size() != headers.size() ||
        id == 0) {
      return false;
    }
    std::vector<InMemoryStoreFieldInput> fields(headers.size());
    for (std::size_t index = 0; index < headers.size(); ++index) {
      fields[index] = InMemoryStoreFieldInput{headers[index], values[index]};
    }
    if (!stores.append_row(store_name, id++, 1, fields.data(), fields.size())) {
      return false;
    }
    ++result.csv_row_count;
  }
}

bool import_file(const char *full_path, const char *store_name,
                 InMemoryStoreBackend &stores, StoreInitLoadResult &result) {
  std::string content;
  if (!read_file(full_path, content)) return false;
  bool imported = false;
  if (has_suffix(store_name, ".lisp")) {
    imported = append_source_lines(store_name, content, stores, result);
  } else if (has_suffix(store_name, ".csv")) {
    imported = append_csv_rows(store_name, content.c_str(), stores, result);
  } else {
    return false;
  }
  if (imported) ++result.store_count;
  return imported;
}

bool import_directory(const char *root, const char *relative,
                      InMemoryStoreBackend &stores, StoreInitLoadResult &result) {
  std::string directory_path;
  if (relative[0] == '\0') {
    try {
      directory_path = root;
    } catch (const std::bad_alloc &) {
      return false;
    }
  } else if (!join_path(directory_path, root, relative)) {
    return false;
  }
  DIR *directory = opendir(directory_path.c_str());
  if (directory == nullptr) return false;

  bool succeeded = true;
  while (succeeded) {
    dirent *entry = readdir(directory);
    if (entry == nullptr) break;
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    std::string child_relative;
    if (relative[0] == '\0') {
      try {
        child_relative = entry->d_name;
      } catch (const std::bad_alloc &) {
        succeeded = false;
        break;
      }
    } else if (!join_path(child_relative, relative, entry->d_name)) {
      succeeded = false;
      break;
    }
    std::string child_path;
    if (!join_path(child_path, root, child_relative)) {
      succeeded = false;
      break;
    }
    struct stat details {};
    if (stat(child_path.c_str(), &details) != 0) {
      succeeded = false;
    } else if (S_ISDIR(details.st_mode)) {
      succeeded = import_directory(root, child_relative.c_str(), stores, result);
    } else if (S_ISREG(details.st_mode)) {
      succeeded = import_file(child_path.c_str(), child_relative.c_str(), stores,
                              result);
    } else {
      succeeded = false;
    }
  }
  return closedir(directory) == 0 && succeeded;
}
}

bool load_store_init(const char *root, InMemoryStoreBackend &stores,
                     StoreInitLoadResult &result) {
  result = StoreInitLoadResult{};
  return root != nullptr && import_directory(root, "", stores, result);
}
