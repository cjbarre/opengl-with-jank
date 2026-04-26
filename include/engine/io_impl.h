#pragma once

namespace eio {
inline char* charify_void (void* buffer) {
  return static_cast<char*>(buffer);
}

inline const char* dirent_name(struct dirent* entry) {
  return entry->d_name;
}
} // namespace eio
