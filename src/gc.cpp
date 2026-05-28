#include "interpreter.hpp"

// ── Thread-local heap pointer ──────────────────────────────────────────────────
// Set to &heap_ at the start of Interpreter::run() and cleared at the end.
// DValue::make_array / make_tuple use this pointer to allocate objects.

thread_local Heap* g_heap = nullptr;

// ── DValue factory methods that require the GC heap ───────────────────────────

DValue DValue::make_array(std::map<long long, DValue> m) {
    DValue d;
    d.type = Type::Array;
    d.href = g_heap->alloc_array(std::move(m));
    return d;
}

DValue DValue::make_tuple(std::vector<TupleElem> e) {
    DValue d;
    d.type = Type::Tuple;
    d.href = g_heap->alloc_tuple(std::move(e));
    return d;
}

// ── Heap implementation ────────────────────────────────────────────────────────

Heap::Heap(size_t threshold) : threshold_{threshold} {}

GcHandle Heap::alloc_array(std::map<long long, DValue> m) {
    auto h = static_cast<GcHandle>(from_.size());
    from_.push_back(HeapObj{ObjKind::Array, GC_NULL, std::move(m), {}});
    return h;
}

GcHandle Heap::alloc_tuple(std::vector<TupleElem> t) {
    auto h = static_cast<GcHandle>(from_.size());
    from_.push_back(HeapObj{ObjKind::Tuple, GC_NULL, {}, std::move(t)});
    return h;
}

// Ensure from_[h] is in to-space; return its new to-space handle.
// Idempotent: if already forwarded, returns the existing forwarding address.
GcHandle Heap::evacuate(GcHandle h) {
    if (h == GC_NULL)
        return GC_NULL;
    HeapObj& obj = from_[h];
    if (obj.fwd != GC_NULL)
        return obj.fwd; // already forwarded

    auto new_h = static_cast<GcHandle>(to_.size());
    to_.push_back(std::move(obj)); // move object to to-space
    to_.back().fwd = GC_NULL;      // clear fwd in the to-space copy
    obj.fwd        = new_h;        // install forwarding pointer in from-space
    return new_h;
}

// Update any GC handles inside v; also trace closure captured-environments.
//
// Closure environments use shared_ptr<Frame> and may be cyclic.  We guard
// against re-entering the same Frame via gc_visited_frames_.
void Heap::scan_value(DValue& v) {
    if (v.type == DValue::Type::Array || v.type == DValue::Type::Tuple) {
        v.href = evacuate(v.href);
    } else if (v.type == DValue::Type::Func && v.fval) {
        for (auto& frame : v.fval->captured_env) {
            if (!gc_visited_frames_.insert(frame.get()).second)
                continue; // frame already scanned this cycle
            for (auto& [name, val] : *frame)
                scan_value(val);
        }
    }
}

// Scan GC-heap children of the already-copied to_[to_idx].
//
// to_.reserve(from_.size()) in collect() prevents reallocation here, so the
// reference obtained via to_[to_idx] remains valid even if push_back is called
// inside evacuate() during recursive scan_value() calls.
void Heap::scan_obj(GcHandle to_idx) {
    if (to_[to_idx].kind == ObjKind::Array) {
        for (auto& [k, v] : to_[to_idx].arr)
            scan_value(v);
    } else {
        for (auto& elem : to_[to_idx].tup)
            scan_value(elem.value);
    }
}

// Stop-the-world Cheney copy collection.
//
// roots      – flat list of pointers to every live DValue (env frames + val_).
// env_frames – the interpreter's current env frames, pre-inserted into
//              gc_visited_frames_ so that closures never re-scan them and
//              cause double-evacuation of the same GC handle.
void Heap::collect(std::vector<DValue*> roots, std::vector<Frame*> env_frames) {
    to_.clear();
    to_.reserve(from_.size()); // live objects ≤ total; no reallocation in phase 2
    gc_visited_frames_.clear();

    // Pre-mark current env frames: closures that capture them will skip re-scan.
    for (Frame* f : env_frames)
        gc_visited_frames_.insert(f);

    // Phase 1: evacuate every root DValue.
    for (DValue* v : roots)
        scan_value(*v);

    // Phase 2: Cheney scan – breadth-first scan of to-space.
    // New objects appended to to_ during scan_obj extend the range.
    for (GcHandle scan = 0; scan < static_cast<GcHandle>(to_.size()); ++scan)
        scan_obj(scan);

    // Phase 3: swap spaces.
    // Unreachable objects remaining in old from_ are freed here by vector dtor.
    from_ = std::move(to_);
    to_.clear();
    gc_visited_frames_.clear();
    ++gc_count_;
}
