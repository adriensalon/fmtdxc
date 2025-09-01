// fmtdxc_impl.cpp
#include <cmath>
#include <ctime>
#include <fmtdxc/fmtdxc.hpp>
#include <utility>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace fmtdxc {

inline bool differs(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) > eps;
}

inline bool differs(float a, float b, float eps = 1e-6f)
{
    return std::fabs(a - b) > eps;
}

template <typename T>
inline bool differs(const T& a, const T& b)
{
    return a != b;
}

template <typename T>
static std::optional<T> diff_value(const T& a, const T& b)
{
    return differs(a, b) ? std::make_optional(b) : std::nullopt;
}

template <typename T>
static void set_if(T& dst, const std::optional<T>& maybe)
{
    if (maybe)
        dst = *maybe;
}

static bool is_empty_audio_clip(const sparse_project::audio_clip& x)
{
    return !x.name && !x.start_tick && !x.length_ticks && !x.file && !x.file_start_frame && !x.db && !x.is_loop;
}

static bool is_empty_midi_mpe(const sparse_project::midi_mpe& x)
{
    return !x.channel && !x.pressure && !x.slide && !x.timbre;
}

static bool is_empty_midi_note(const sparse_project::midi_note& x)
{
    return !x.start_tick && !x.length_ticks && !x.pitch && !x.velocity && (!x.mpe || is_empty_midi_mpe(*x.mpe));
}

static bool is_empty_midi_clip(const sparse_project::midi_clip& x)
{
    return !x.name && !x.start_tick && !x.length_ticks && x.notes.empty();
}

static bool is_empty_audio_sequencer(const sparse_project::audio_sequencer& x)
{
    return !x.name && !x.output && x.clips.empty();
}

static bool is_empty_midi_sequencer(const sparse_project::midi_sequencer& x)
{
    // we only consider instrument.name at the moment
    return !x.name && !x.output && (!x.instrument || !x.instrument->name) && x.clips.empty();
}

static bool is_empty_mixer_track(const sparse_project::mixer_track& x)
{
    return !x.name && !x.db && !x.pan && x.effects.empty() && x.routings.empty();
}

static bool is_empty(const sparse_project& x)
{
    return !x.name && !x.ppq && !x.master_track_id && x.audio_sequencers.empty() && x.midi_sequencers.empty() && x.mixer_tracks.empty();
}

static sparse_project::audio_clip full_patch_audio_clip(const project::audio_clip& s)
{
    sparse_project::audio_clip p;
    p.name = s.name;
    p.start_tick = s.start_tick;
    p.length_ticks = s.length_ticks;
    p.file = s.file;
    p.file_start_frame = s.file_start_frame;
    p.db = s.db;
    p.is_loop = s.is_loop;
    return p;
}

static sparse_project::midi_mpe full_patch_midi_mpe(const project::midi_mpe& s)
{
    sparse_project::midi_mpe p;
    p.channel = s.channel;
    p.pressure = s.pressure;
    p.slide = s.slide;
    p.timbre = s.timbre;
    return p;
}

static sparse_project::midi_note full_patch_midi_note(const project::midi_note& s)
{
    sparse_project::midi_note p;
    p.start_tick = s.start_tick;
    p.length_ticks = s.length_ticks;
    p.pitch = s.pitch;
    p.velocity = s.velocity;
    // if (s.mpe) p.mpe = full_patch(*s.mpe);
    return p;
}

static sparse_project::midi_clip full_patch_midi_clip(const project::midi_clip& s)
{
    sparse_project::midi_clip p;
    p.name = s.name;
    p.start_tick = s.start_tick;
    p.length_ticks = s.length_ticks;
    for (auto& [nid, n] : s.notes)
        p.notes.emplace(nid, full_patch_midi_note(n));
    return p;
}

static sparse_project::audio_sequencer full_patch_audio_sequencer(const project::audio_sequencer& s)
{
    sparse_project::audio_sequencer p;
    p.name = s.name;
    p.output = s.output;
    for (auto& [cid, c] : s.clips)
        p.clips.emplace(cid, full_patch_audio_clip(c));
    return p;
}

static sparse_project::midi_sequencer full_patch_midi_sequencer(const project::midi_sequencer& s)
{
    sparse_project::midi_sequencer p;
    p.name = s.name;
    p.output = s.output;
    p.instrument = sparse_project::midi_instrument {};
    p.instrument->name = s.instrument.name;
    for (auto& [cid, c] : s.clips)
        p.clips.emplace(cid, full_patch_midi_clip(c));
    return p;
}

static sparse_project::mixer_track full_patch_mixer_track(const project::mixer_track& s)
{
    sparse_project::mixer_track p;
    p.name = s.name;
    p.db = s.db;
    p.pan = s.pan;
    // effects/routings are placeholders in your model; include when you model fields there
    return p;
}

static sparse_project::audio_clip diff_audio_clip(const project::audio_clip& a, const project::audio_clip& b)
{
    sparse_project::audio_clip out;
    out.name = diff_value(a.name, b.name);
    out.start_tick = diff_value(a.start_tick, b.start_tick);
    out.length_ticks = diff_value(a.length_ticks, b.length_ticks);
    out.file = diff_value(a.file, b.file);
    out.file_start_frame = diff_value(a.file_start_frame, b.file_start_frame);
    out.db = diff_value(a.db, b.db);
    out.is_loop = diff_value(a.is_loop, b.is_loop);
    return out;
}

static sparse_project::midi_note diff_midi_note(const project::midi_note& a, const project::midi_note& b)
{
    sparse_project::midi_note out;
    out.start_tick = diff_value(a.start_tick, b.start_tick);
    out.length_ticks = diff_value(a.length_ticks, b.length_ticks);
    out.pitch = diff_value(a.pitch, b.pitch);
    out.velocity = diff_value(a.velocity, b.velocity);
    // optional: field-level MPE diff
    // if (a.mpe != b.mpe) {
    //     sparse_project::midi_mpe mpe;
    //     if (a.mpe && b.mpe) {
    //         mpe.channel  = diff_value(a.mpe.channel,  b.mpe.channel);
    //         mpe.pressure = diff_value(a.mpe.pressure, b.mpe.pressure);
    //         mpe.slide    = diff_value(a.mpe.slide,    b.mpe.slide);
    //         mpe.timbre   = diff_value(a.mpe.timbre,   b.mpe.timbre);
    //         if (!is_empty(mpe)) out.mpe = mpe;
    //     } else if (b.mpe) {
    //         out.mpe = full_patch(*b.mpe);
    //     } else {
    //         // deletion of mpe not modeled; skip or set out.mpe = sparse_project::midi_mpe{} (no-op)
    //     }
    // }
    return out;
}

// generic diff_map with pruning + proper adds
template <typename K, typename V, typename DiffFn, typename FullPatchFn, typename IsEmptyFn>
static auto diff_map(const std::map<K, V>& a, const std::map<K, V>& b,
    DiffFn&& diff_entity, FullPatchFn&& full_entity, IsEmptyFn&& is_empty_entity)
    -> std::map<K, decltype(diff_entity(std::declval<V>(), std::declval<V>()))>
{
    using SparseV = decltype(diff_entity(std::declval<V>(), std::declval<V>()));
    std::map<K, SparseV> out;

    for (auto& [idB, objB] : b) {
        auto itA = a.find(idB);
        if (itA == a.end()) {
            auto patch = full_entity(objB); // NEW entity => full payload
            if (!is_empty_entity(patch))
                out.emplace(idB, std::move(patch));
        } else {
            auto patch = diff_entity(itA->second, objB);
            if (!is_empty_entity(patch))
                out.emplace(idB, std::move(patch));
        }
    }
    // deletes not modeled (by design). If you need them, switch to optional<T> map values in sparse mode.
    return out;
}

static sparse_project::midi_clip diff_midi_clip(const project::midi_clip& a, const project::midi_clip& b)
{
    sparse_project::midi_clip out;
    out.name = diff_value(a.name, b.name);
    out.start_tick = diff_value(a.start_tick, b.start_tick);
    out.length_ticks = diff_value(a.length_ticks, b.length_ticks);
    out.notes = diff_map(a.notes, b.notes,
        diff_midi_note,
        full_patch_midi_note,
        is_empty_midi_note);
    return out;
}

static sparse_project::audio_sequencer diff_audio_sequencer(const project::audio_sequencer& a, const project::audio_sequencer& b)
{
    sparse_project::audio_sequencer out;
    out.name = diff_value(a.name, b.name);
    out.output = diff_value(a.output, b.output);
    out.clips = diff_map(a.clips, b.clips,
        diff_audio_clip,
        full_patch_audio_clip,
        is_empty_audio_clip);
    return out;
}

static sparse_project::midi_sequencer diff_midi_sequencer(const project::midi_sequencer& a, const project::midi_sequencer& b)
{
    sparse_project::midi_sequencer out;
    out.name = diff_value(a.name, b.name);
    out.output = diff_value(a.output, b.output);
    // instrument (only name at the moment)
    if (a.instrument.name != b.instrument.name) {
        out.instrument = sparse_project::midi_instrument {};
        out.instrument->name = b.instrument.name;
    }
    out.clips = diff_map(a.clips, b.clips,
        diff_midi_clip,
        full_patch_midi_clip,
        is_empty_midi_clip);
    return out;
}

static sparse_project::mixer_track diff_mixer_track(const project::mixer_track& a, const project::mixer_track& b)
{
    sparse_project::mixer_track out;
    out.name = diff_value(a.name, b.name);
    out.db = diff_value(a.db, b.db);
    out.pan = diff_value(a.pan, b.pan);
    // effects / routings can be added here when fields exist
    return out;
}

void diff(const project& a, const project& b, sparse_project& out)
{
    out.name = diff_value(a.name, b.name);
    out.ppq = diff_value(a.ppq, b.ppq);
    out.master_track_id = diff_value(a.master_track_id, b.master_track_id);

    out.audio_sequencers = diff_map(a.audio_sequencers, b.audio_sequencers,
        diff_audio_sequencer,
        full_patch_audio_sequencer,
        is_empty_audio_sequencer);
    out.midi_sequencers = diff_map(a.midi_sequencers, b.midi_sequencers,
        diff_midi_sequencer,
        full_patch_midi_sequencer,
        is_empty_midi_sequencer);
    out.mixer_tracks = diff_map(a.mixer_tracks, b.mixer_tracks,
        diff_mixer_track,
        full_patch_mixer_track,
        is_empty_mixer_track);
}

static void apply_audio_clip(project::audio_clip& dst, const sparse_project::audio_clip& p)
{
    set_if(dst.name, p.name);
    set_if(dst.start_tick, p.start_tick);
    set_if(dst.length_ticks, p.length_ticks);
    set_if(dst.file, p.file);
    set_if(dst.file_start_frame, p.file_start_frame);
    set_if(dst.db, p.db);
    set_if(dst.is_loop, p.is_loop);
}

static void apply_midi_mpe(project::midi_mpe& dst, const sparse_project::midi_mpe& p)
{
    set_if(dst.channel, p.channel);
    set_if(dst.pressure, p.pressure);
    set_if(dst.slide, p.slide);
    set_if(dst.timbre, p.timbre);
}

static void apply_midi_note(project::midi_note& dst, const sparse_project::midi_note& p)
{
    set_if(dst.start_tick, p.start_tick);
    set_if(dst.length_ticks, p.length_ticks);
    set_if(dst.pitch, p.pitch);
    set_if(dst.velocity, p.velocity);
    // apply_midi_mpe(dst.mpe, p.mpe);
}

static void apply_midi_clip(project::midi_clip& dst, const sparse_project::midi_clip& p)
{
    set_if(dst.name, p.name);
    set_if(dst.start_tick, p.start_tick);
    set_if(dst.length_ticks, p.length_ticks);
    for (auto& [nid, np] : p.notes) {
        auto& note = dst.notes[nid]; // create if missing
        apply_midi_note(note, np);
    }
}

static void apply_audio_sequencer(project::audio_sequencer& dst, const sparse_project::audio_sequencer& p)
{
    set_if(dst.name, p.name);
    set_if(dst.output, p.output);
    for (auto& [cid, cp] : p.clips) {
        auto& clip = dst.clips[cid]; // create if missing
        apply_audio_clip(clip, cp);
    }
}

static void apply_midi_sequencer(project::midi_sequencer& dst, const sparse_project::midi_sequencer& p)
{
    set_if(dst.name, p.name);
    set_if(dst.output, p.output);
    if (p.instrument) {
        set_if(dst.instrument.name, p.instrument->name);
    }
    for (auto& [cid, cp] : p.clips) {
        auto& clip = dst.clips[cid];
        apply_midi_clip(clip, cp);
    }
}

static void apply_mixer_track(project::mixer_track& dst, const sparse_project::mixer_track& p)
{
    set_if(dst.name, p.name);
    set_if(dst.db, p.db);
    set_if(dst.pan, p.pan);
    // effects/routings once modeled
}

void apply(const project& base, const sparse_project& diffs, project& out)
{
    out = base;

    set_if(out.name, diffs.name);
    set_if(out.ppq, diffs.ppq);
    set_if(out.master_track_id, diffs.master_track_id);

    for (auto& [asid, asp] : diffs.audio_sequencers) {
        auto& as = out.audio_sequencers[asid]; // add or modify
        apply_audio_sequencer(as, asp);
    }
    for (auto& [msid, msp] : diffs.midi_sequencers) {
        auto& ms = out.midi_sequencers[msid];
        apply_midi_sequencer(ms, msp);
    }
    for (auto& [mtid, mtp] : diffs.mixer_tracks) {
        auto& mt = out.mixer_tracks[mtid];
        apply_mixer_track(mt, mtp);
    }
}

void apply(project& base, const sparse_project& diffs)
{
    project _out;
    apply(base, diffs, _out);
    base = _out;
    // fugly!
}

// ---------- project_container methods ----------
project_container::project_container()
    : _proj {}
    , _applied(0)
{
}
project_container::project_container(const project& base)
    : _proj(base)
    , _applied(0)
{
}
project_container::project_container(const project& base,
    const std::vector<project_commit>& commits)
    : _proj(base)
    , _applied(0)
    , _commits(commits)
{
}
project_container::project_container(const project& base,
    const std::vector<project_commit>& commits,
    const std::size_t applied)
    : _proj(base)
    , _applied(applied)
    , _commits(commits)
{
}

bool project_container::can_undo() const { return _applied > 0; }

bool project_container::can_redo() const { return _applied < _commits.size(); }

std::size_t project_container::get_applied_count() const { return _applied; }

const project& project_container::get_project() const { return _proj; }

const std::vector<project_commit>& project_container::get_commits() const { return _commits; }

void project_container::commit(const std::string& message, const project& next)
{
    // truncate redo tail if any
    if (_applied < _commits.size())
        _commits.resize(_applied);

    project_commit c;
    c.message = message;
    c.timestamp = std::time(nullptr);
    diff(_proj, next, c.forward);
    diff(next, _proj, c.backward);

    // skip no-op commits
    if (is_empty(c.forward)) {
        _proj = next;
        return;
    }

#ifndef NDEBUG
    {
        project after, rewind;
        apply(_proj, c.forward, after);
        apply(after, c.backward, rewind);
        // optional sanity: if (rewind != _proj) { /*log*/ }
        (void)rewind;
    }
#endif

    _commits.push_back(std::move(c));
    ++_applied;
    _proj = next;
}

void project_container::undo()
{
    if (!can_undo())
        return;
    const auto& c = _commits[_applied - 1];
    apply(_proj, c.backward);
    --_applied;
}

void project_container::redo()
{
    if (!can_redo())
        return;
    const auto& c = _commits[_applied];
    apply(_proj, c.forward);
    ++_applied;
}
}

// SERIALIZATION
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
namespace std {
namespace filesystem {

    template <typename archive_t>
    void serialize(archive_t& archive, path& p)
    {
        if constexpr (archive_t::is_saving::value) {
            archive(cereal::make_nvp("path", p.string()));
        } else if constexpr (archive_t::is_loading::value) {
            std::string _str;
            archive(cereal::make_nvp("path", _str));
            p = std::filesystem::path(_str);
        }
    }

}
}

// // Forward decls
// template <class Archive> void serialize_impl(Archive&, basic_project<false>::mixer_track&);
// template <class Archive> void serialize_impl(Archive&, basic_project<true>::mixer_track&);

// // Generic dispatcher: matches anything, then forwards
// template <class Archive, class T>
// void serialize(Archive& ar, T& v) {
//     serialize_impl(ar, v);
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::audio_effect& value)
// {
//     archive(cereal::make_nvp("name", value.name));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::midi_instrument& value)
// {
//     archive(cereal::make_nvp("name", value.name));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::audio_clip& value)
// {
//     archive(cereal::make_nvp("name", value.name));
//     archive(cereal::make_nvp("start_tick", value.start_tick));
//     archive(cereal::make_nvp("length_ticks", value.length_ticks));
//     //
//     archive(cereal::make_nvp("file_start_frame", value.file_start_frame));
//     archive(cereal::make_nvp("db", value.db));
//     archive(cereal::make_nvp("is_loop", value.is_loop));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::midi_mpe& value)
// {
//     archive(cereal::make_nvp("channel", value.channel));
//     archive(cereal::make_nvp("pressure", value.pressure));
//     archive(cereal::make_nvp("slide", value.slide));
//     archive(cereal::make_nvp("timbre", value.timbre));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::midi_note& value)
// {
//     archive(cereal::make_nvp("start_tick", value.start_tick));
//     archive(cereal::make_nvp("length_ticks", value.length_ticks));
//     archive(cereal::make_nvp("pitch", value.pitch));
//     archive(cereal::make_nvp("velocity", value.velocity));
//     archive(cereal::make_nvp("mpe", value.mpe));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::midi_clip& value)
// {
//     archive(cereal::make_nvp("name", value.name));
//     archive(cereal::make_nvp("start_tick", value.start_tick));
//     archive(cereal::make_nvp("length_ticks", value.length_ticks));
//     archive(cereal::make_nvp("notes", value.notes));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::audio_sequencer& value)
// {
//     archive(cereal::make_nvp("name", value.name));
//     archive(cereal::make_nvp("clips", value.clips));
//     archive(cereal::make_nvp("output", value.output));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::midi_sequencer& value)
// {
//     archive(cereal::make_nvp("name", value.name));
//     archive(cereal::make_nvp("instrument", value.instrument));
//     archive(cereal::make_nvp("clips", value.clips));
//     archive(cereal::make_nvp("output", value.output));
// }

// template <typename archive_t, bool sparse_t>
// void serialize_impl(archive_t& archive, typename basic_project<sparse_t>::mixer_routing& value)
// {
//     archive(cereal::make_nvp("db", value.db));
//     archive(cereal::make_nvp("output", value.output));
// }

#define FMX_SERIALIZE_NESTED(Nested, BODY)                              \
    template <class archive_t>                                          \
    void serialize(archive_t& archive, project::Nested& value) { BODY } \
    template <class archive_t>                                          \
    void serialize(archive_t& archive, sparse_project::Nested& value) { BODY }

namespace fmtdxc {

FMX_SERIALIZE_NESTED(audio_effect, {
    archive(cereal::make_nvp("name", value.name));
});

FMX_SERIALIZE_NESTED(midi_instrument, {
    archive(cereal::make_nvp("name", value.name));
});

FMX_SERIALIZE_NESTED(audio_clip, {
    archive(cereal::make_nvp("name", value.name));
    archive(cereal::make_nvp("start_tick", value.start_tick));
    archive(cereal::make_nvp("length_ticks", value.length_ticks));
    // file
    archive(cereal::make_nvp("file_start_frame", value.file_start_frame));
    archive(cereal::make_nvp("db", value.db));
    archive(cereal::make_nvp("is_loop", value.is_loop));
});

FMX_SERIALIZE_NESTED(midi_mpe, {
    archive(cereal::make_nvp("channel", value.channel));
    archive(cereal::make_nvp("pressure", value.pressure));
    archive(cereal::make_nvp("slide", value.slide));
    archive(cereal::make_nvp("timbre", value.timbre));
});

FMX_SERIALIZE_NESTED(midi_note, {
    archive(cereal::make_nvp("start_tick", value.start_tick));
    archive(cereal::make_nvp("length_ticks", value.length_ticks));
    archive(cereal::make_nvp("pitch", value.pitch));
    archive(cereal::make_nvp("velocity", value.velocity));
    archive(cereal::make_nvp("mpe", value.mpe));
});

FMX_SERIALIZE_NESTED(midi_clip, {
    archive(cereal::make_nvp("name", value.name));
    archive(cereal::make_nvp("start_tick", value.start_tick));
    archive(cereal::make_nvp("length_ticks", value.length_ticks));
    archive(cereal::make_nvp("notes", value.notes));
});

FMX_SERIALIZE_NESTED(audio_sequencer, {
    archive(cereal::make_nvp("name", value.name));
    archive(cereal::make_nvp("instrument", value.instrument));
    archive(cereal::make_nvp("clips", value.clips));
    archive(cereal::make_nvp("output", value.output));
});

FMX_SERIALIZE_NESTED(mixer_routing, {
    archive(cereal::make_nvp("db", value.db));
    archive(cereal::make_nvp("output", value.output));
});

FMX_SERIALIZE_NESTED(mixer_track, {
    archive(cereal::make_nvp("name", value.name));
    archive(cereal::make_nvp("db", value.db));
    archive(cereal::make_nvp("pan", value.pan));
    archive(cereal::make_nvp("effects", value.effects));
    archive(cereal::make_nvp("routings", value.routings));
});

template <typename archive_t, bool sparse_t>
void serialize(archive_t& archive, basic_project<sparse_t>& value)
{
    archive(cereal::make_nvp("name", value.name));
    archive(cereal::make_nvp("ppq", value.ppq));
    // archive(cereal::make_nvp("audio_sequencers", value.audio_sequencers));
    // archive(cereal::make_nvp("midi_sequencers", value.midi_sequencers));
    archive(cereal::make_nvp("mixer_tracks", value.mixer_tracks));
    archive(cereal::make_nvp("master_track_id", value.master_track_id));
}

template <typename archive_t>
void serialize(archive_t& archive, project_commit& value)
{
    archive(cereal::make_nvp("message", value.message));
    archive(cereal::make_nvp("timestamp", value.timestamp));
    archive(cereal::make_nvp("forward", value.forward));
    archive(cereal::make_nvp("backward", value.backward));
}

template <typename archive_t>
void serialize(archive_t& archive, project_container& value)
{
    archive(cereal::make_nvp("project", value._proj));
    archive(cereal::make_nvp("applied", value._applied));
    archive(cereal::make_nvp("commits", value._commits));
}

void import_container(std::istream& stream, project_container& container, version& ver, const bool as_json)
{
    if (as_json) {
        cereal::JSONInputArchive _archive(stream);
        _archive(cereal::make_nvp("dawxchange json container", container));
    } else {
        cereal::BinaryInputArchive _archive(stream);
        _archive(cereal::make_nvp("dawxchange binary container", container));
    }
}

void export_container(std::ostream& stream, const project_container& container, const version& ver, const bool as_json)
{
    if (as_json) {
        cereal::JSONOutputArchive _archive(stream);
        _archive(cereal::make_nvp("dawxchange json container", container));
    } else {
        cereal::BinaryOutputArchive _archive(stream);
        _archive(cereal::make_nvp("dawxchange binary container", container));
    }
}
}
