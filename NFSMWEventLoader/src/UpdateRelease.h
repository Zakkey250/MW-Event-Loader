#pragma once
#include <array>
#include <charconv>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "../../NFSMWNativeFreeRoamRacers/third_party/nlohmann/json.hpp"

// Reusable, game-independent metadata policy. Never consume remote HTML, text,
// executable URLs, or version strings as commands or local file names.
namespace mod_update {
struct Version {
    unsigned major=0,minor=0,patch=0,stage=3,number=0;
    auto Key() const {return std::array<unsigned,5>{major,minor,patch,stage,number};}
};
inline std::optional<Version> ParseVersion(std::string_view value) {
    if(value.empty()||value.size()>64)return {};
    if(value.front()=='v')value.remove_prefix(1);
    auto number=[&](unsigned& out){
        const char* start=value.data();const char* end=start;
        while(end<start+value.size()&&*end>='0'&&*end<='9')++end;
        if(end==start||(end-start>1&&*start=='0'))return false;
        auto parsed=std::from_chars(start,end,out);
        if(parsed.ec!=std::errc{}||out>1000000)return false;
        value.remove_prefix(static_cast<std::size_t>(end-start));return true;
    };
    auto consume=[&](std::string_view prefix){if(!(value.substr(0,prefix.size())==prefix))return false;value.remove_prefix(prefix.size());return true;};
    Version result;
    if(!number(result.major)||!consume(".")||!number(result.minor)||!consume(".")||!number(result.patch))return {};
    if(value.empty())return result;
    if(consume("-alpha."))result.stage=0;
    else if(consume("-beta."))result.stage=1;
    else if(consume("-rc."))result.stage=2;
    else return {};
    if(!number(result.number)||!value.empty())return {};
    return result;
}
struct Release {std::string tag;Version version;};
inline std::optional<Release> FindUpdate(std::string_view payload,std::string_view current,
                                        std::string_view repository,std::string_view assetPrefix) {
    const auto installed=ParseVersion(current);
    if(!installed||payload.empty()||payload.size()>2*1024*1024)return {};
    try {
        // Bound depth and reject duplicate keys instead of accepting the last
        // value of a potentially ambiguous version/draft/asset field.
        std::vector<std::set<std::string>> keys;
        const auto callback=[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& node){
            if(depth>32)throw std::runtime_error("JSON depth");
            if(event==nlohmann::json::parse_event_t::object_start)keys.emplace_back();
            if(event==nlohmann::json::parse_event_t::key&&(!keys.size()||!keys.back().insert(node.get<std::string>()).second))throw std::runtime_error("JSON duplicate key");
            if(event==nlohmann::json::parse_event_t::object_end)keys.pop_back();
            return true;
        };
        auto document=nlohmann::json::parse(payload.begin(),payload.end(),callback);
        if(!document.is_array()||document.size()>100)return {};
        std::optional<Release> best;
        for(const auto& row:document) {
            if(!row.is_object()||!row.contains("draft")||!row["draft"].is_boolean()||row["draft"].get<bool>()||
               !row.contains("prerelease")||!row["prerelease"].is_boolean()||
               !row.contains("tag_name")||!row["tag_name"].is_string()||
               !row.contains("published_at")||!row["published_at"].is_string()||row["published_at"].get_ref<const std::string&>().empty()||
               !row.contains("html_url")||!row["html_url"].is_string()||
               !row.contains("assets")||!row["assets"].is_array())continue;
            auto tag=row["tag_name"].get<std::string>();
            if(tag.empty()||tag[0]!='v')continue;
            const auto candidate=ParseVersion(tag);
            if(!candidate||candidate->Key()<=installed->Key()||(best&&candidate->Key()<=best->version.Key()))continue;
            const bool prerelease=row["prerelease"].get<bool>();
            if(prerelease!=(candidate->stage!=3)||(installed->stage==3&&prerelease))continue;
            const std::string url="https://github.com/"+std::string(repository)+"/releases/tag/"+tag;
            if(row["html_url"].get_ref<const std::string&>()!=url)continue;
            const std::string expected=std::string(assetPrefix)+tag+".zip";
            bool binary=false;
            for(const auto& asset:row["assets"]) {
                if(!asset.is_object()||!asset.contains("name")||!asset["name"].is_string()||asset["name"].get_ref<const std::string&>()!=expected||
                   !asset.contains("state")||asset["state"]!="uploaded"||!asset.contains("size")||!asset["size"].is_number_unsigned()||asset["size"].get<std::uint64_t>()==0)continue;
                binary=true;break;
            }
            if(binary)best=Release{std::move(tag),*candidate};
        }
        return best;
    }catch(...){return {};}
}
inline bool Japanese(std::wstring_view language) {
    auto end=language.find(L';');if(end!=std::wstring_view::npos)language=language.substr(0,end);
    auto first=language.find_first_not_of(L" \r\n\t");if(first==std::wstring_view::npos)return false;
    language=language.substr(first,language.find_last_not_of(L" \r\n\t")-first+1);
    std::wstring normalized(language);for(auto& c:normalized)if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
    return normalized==L"ja"||normalized==L"jp"||normalized==L"japanese"||normalized==L"ja-jp";
}
}
