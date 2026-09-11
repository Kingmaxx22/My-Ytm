#pragma once

#include "models/album.h"
#include "models/artist.h"
#include "models/playlist.h"
#include "models/song.h"

#include <string>
#include <variant>
#include <vector>

namespace myytm::models {

enum class SearchResultType {
    Song,
    Artist,
    Album,
    Playlist,
};

struct SearchResult {
    SearchResultType type;
    std::string id;
    std::string title;
    std::string subtitle; // artist / author / etc.

    // Optional payload — lightweight; full objects can be lazy-loaded in later phases.
    std::variant<std::monostate, Song, Artist, Album, Playlist> payload;

    [[nodiscard]] std::string typeLabel() const
    {
        switch (type) {
            case SearchResultType::Song: return "Song";
            case SearchResultType::Artist: return "Artist";
            case SearchResultType::Album: return "Album";
            case SearchResultType::Playlist: return "Playlist";
        }
        return "?";
    }

    [[nodiscard]] std::string display() const
    {
        if (subtitle.empty()) return title;
        return title + " — " + subtitle;
    }

    static SearchResult fromSong(const Song& s)
    {
        return {SearchResultType::Song, s.id, s.title, s.artist, s};
    }
    static SearchResult fromArtist(const Artist& a)
    {
        return {SearchResultType::Artist, a.id, a.name, "", a};
    }
    static SearchResult fromAlbum(const Album& a)
    {
        return {SearchResultType::Album, a.id, a.title, a.artist, a};
    }
    static SearchResult fromPlaylist(const Playlist& p)
    {
        return {SearchResultType::Playlist, p.id, p.title, p.author, p};
    }
};

using SearchResults = std::vector<SearchResult>;

} // namespace myytm::models
