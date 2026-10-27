#pragma once
#include <filesystem>
namespace event_loader_update {
inline constexpr char Version[]="1.0.3";
// Schedules one bounded worker; never performs HTTP/UI under the loader lock.
void StartNotices(const std::filesystem::path& ini) noexcept;
}
