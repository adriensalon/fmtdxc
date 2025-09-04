#pragma once

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace fmtdxc {

/// @brief Represents dawxchange version with an enum that will not collide with versions from other DAWs.
/// dawxchange id is defined as 99. Currently in alpha
enum struct version : unsigned int {
    alpha = 90000
};

/// @brief Abstract class for dawxchange projects.
/// @tparam sparse_t allows for replacing T fields with std::optional<T> for merge operations
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

    struct collected_audio_file {
        value<std::vector<char>> data;
        value<std::filesystem::path> collected_relative_path;
    };

    struct audio_clip {
        value<std::string> name;
        value<std::uint64_t> start_tick;
        value<std::uint64_t> length_ticks;
        value<std::filesystem::path> file; // replace w std::variant<std::filesystem::path, audio_file>
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

/// @brief Represents a dawxchange project
using project = basic_project<false>;

/// @brief Represents a sparse dawxchange project for merge operations
using sparse_project = basic_project<true>;

/// @brief Create a diff from dawxchange projects
/// @param base Dawxchange project to compare from
/// @param other Dawxchange project to compare to
/// @param result Sparse dawxchange project containing diffed fields only
void diff(const project& base, const project& other, sparse_project& result);

/// @brief Applies changes from a sparse dawxchange project to a dawxhange project
/// @param base Dawxchange project to apply to
/// @param diffs Sparse dawxchange project to apply from
/// @param result Dawxchange project with applied changes
void apply(const project& base, const sparse_project& diffs, project& result);

/// @brief Applies changes inplace from a sparse dawxchange project to a dawxhange project
/// @param base Dawxchange project to inplace apply to
/// @param diffs Sparse dawxchange project to apply from
void apply(project& base, const sparse_project& diffs);

/// @brief Represents changes to a dawxchange project as optimized data and metadata
struct project_commit {
    std::string message;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    sparse_project forward;
    sparse_project backward;
};

/// @brief Represents a feature rich dawxchange project that saves changes history as linear commits.
/// This is the format to use from daws
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
    void commit(const project_commit& next);
    void undo();
    void redo();

private:
    project _proj;
    std::size_t _applied;
    std::vector<project_commit> _commits;
    friend void scan_project(const project_container& container, struct project_info& info);

    template <typename archive_t>
    friend void serialize(archive_t& archive, project_container& value);
};

/// @brief Imports a project container from an input stream
/// @param stream Input stream to import from
/// @param container Project container to import to
/// @param ver Detected version of the project container
void import_container(std::istream& stream, project_container& container, version& ver, const bool as_json = false);

/// @brief Exports a project container to an output stream
/// @param stream Output stream to export to
/// @param container Project container to export from
/// @param ver Choosen dawxchange version of the project container
void export_container(std::ostream& stream, const project_container& container, const version& ver, const bool as_json = false);

/// @brief Represents metadata about a container
struct project_info {
    std::chrono::time_point<std::chrono::system_clock> created_on;
    std::chrono::time_point<std::chrono::system_clock> modified_on;
    std::vector<project_commit> commits;
    std::size_t applied;
};

/// @brief Scans a container for metadata
/// @param container Container to scan from
/// @param info Resulting container metadata
void scan_project(const project_container& container, project_info& info);

/// @brief Scans a container path for metadata
/// @param container_path Container path to scan from
/// @param info Resulting container metadata
void scan_project(const std::filesystem::path& container_path, project_info& info);

}