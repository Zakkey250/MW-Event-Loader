#pragma once
#include <filesystem>
namespace mwel_paths {
namespace fs=std::filesystem;
inline fs::path Root(const fs::path& game){return game/L"scripts"/L"EventLoader";}
inline fs::path Diagnostic(const fs::path& game,const wchar_t* canonical){return Root(game)/canonical;}
inline fs::path Settings(const fs::path& game){return Root(game)/L"EventLoader.ini";}
inline fs::path Events(const fs::path& game){return Root(game)/L"CustomEvents";}
inline fs::path Scenes(const fs::path& game){return Root(game)/L"CustomCutscenes";}
inline fs::path Profiles(const fs::path& game){return Root(game)/L"Profiles";}
inline fs::path Geometry(const fs::path& game){return Root(game)/L"WorldGeometry";}
}
