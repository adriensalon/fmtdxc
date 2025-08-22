#pragma once

#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace fmtdxc {

/// @brief
enum struct version : unsigned int {
    alpha
};

template <bool sparse_t = false>
struct basic_project {

    template <typename T>
    using value = std::conditional_t<sparse_t, std::optional<T>, T>;

    template <typename T>
    using id = std::conditional_t<sparse_t, std::optional<std::uint32_t>, std::uint32_t>;

    template <typename T>
    using id_map = std::map<std::uint32_t, T>;

    struct audio_effect {
        value<std::string> name;
        // placeholder
    };

    struct midi_instrument {
        value<std::string> name;
        // placeholder
    };

    struct audio_clip {
        value<std::string> name;
        value<std::uint64_t> start_tick;
        value<std::uint64_t> length_ticks;
        value<std::filesystem::path> file;
        value<std::uint64_t> file_start_frame;
        value<double> db;
        value<bool> is_loop;
    };

    struct midi_mpe {
        value<std::uint32_t> channel;
        value<float> pressure = 0.0f; // Z
        value<float> slide = 0.0f; // Y
        value<float> timbre = 0.0f; // X
    };

    struct midi_note {
        value<std::uint64_t> start_tick;
        value<std::uint64_t> length_ticks;
        value<std::uint16_t> pitch;
        value<float> velocity;
        value<midi_mpe> mpe;
    };

    struct midi_clip {
        value<std::string> name;
        value<std::uint64_t> start_tick;
        value<std::uint64_t> length_ticks;
        id_map<midi_note> notes;
    };

    struct mixer_track;

    struct audio_sequencer {
        value<std::string> name;
        id_map<audio_clip> clips;
        id<mixer_track> output;
    };

    struct midi_sequencer {
        value<std::string> name;
        value<midi_instrument> instrument;
        id_map<midi_clip> clips;
        id<mixer_track> output;
    };

    struct mixer_routing {
        value<double> db;
        id<mixer_track> output;
    };

    struct mixer_track {
        value<std::string> name;
        value<double> db;
        value<double> pan;
        id_map<audio_effect> effects;
        id_map<mixer_routing> routings;
    };

    value<std::string> name;
    value<std::uint32_t> ppq;
    id_map<audio_sequencer> audio_sequencers;
    id_map<midi_sequencer> midi_sequencers;
    id_map<mixer_track> mixer_tracks;
    id<mixer_track> master_track_id;
};

/// @brief
using project = basic_project<false>;

/// @brief
using sparse_project = basic_project<true>;

/// @brief
/// @param base
/// @param other
/// @return
[[nodiscard]] sparse_project diff(const project& base, const project& other);

/// @brief
/// @param base
/// @param diffs
/// @return
[[nodiscard]] project apply(const project& base, const sparse_project& diffs);

/// @brief
struct project_commit {
    std::string message;
    std::time_t timestamp;
    sparse_project forward;
    sparse_project backward;
};

/// @brief
struct project_container {
    project_container();
    project_container(const project& base);
    project_container(const project& base, const std::vector<project_commit>& commits);
    project_container(const project& base, const std::vector<project_commit>& commits, const std::size_t applied);
    project_container(const project_container& other) = delete;
    project_container& operator=(const project_container& other) = delete;
    project_container(project_container&& other) = default;
    project_container& operator=(project_container&& other) = default;

    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t get_applied_count() const;
    [[nodiscard]] const project& get_project() const;
    [[nodiscard]] const std::vector<project_commit>& get_commits() const;
    void commit(const std::string& message, const project& next);
    void undo();
    void redo();

private:
    project _proj;
    std::size_t _applied;
    std::vector<project_commit> _commits;
};

/// @brief
/// @param stream
/// @param proj
/// @param ver
void import_project_container(std::istream& stream, project_container& proj, version& ver);

/// @brief
/// @param stream
/// @param proj
/// @param ver
void export_project_container(std::ostream& stream, const project_container& proj, const version& ver);

/// @brief 
struct project_commit_info {
    std::string message;
    std::time_t timestamp;
};

/// @brief 
struct project_info {
    std::filesystem::path absolute;
    std::string name;
    std::time_t created;
    std::time_t last_modified;
    std::uint32_t ppq;
    std::size_t audio_clips_count;
    std::size_t midi_clips_count;
    std::size_t audio_sequencers_count;
    std::size_t midi_sequencers_count;
    std::size_t mixer_tracks_count;
    std::vector<project_commit_info> commits;
    std::size_t commits_applied; 
};

/// @brief
/// @param root
/// @return
[[nodiscard]] project_info scan_project(const std::filesystem::path& project_path);

}