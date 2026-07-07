#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>

namespace oop {

// Read a small binary asset (for example a PNG/JPEG) and encode it for the
// Ollama multimodal message format. The function is deliberately separate
// from OllamaClient so the client remains responsible only for HTTP/JSON.
Result<std::string> read_file_base64(const std::filesystem::path& path);

}  // namespace oop
