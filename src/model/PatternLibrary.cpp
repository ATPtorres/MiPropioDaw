#include "PatternLibrary.h"
#include "../core/IDs.h"

namespace
{
    constexpr const char* kDefaultPatternName = "Pattern";
    constexpr int kMaxNotes = 2048;
}

juce::ValueTree PatternLibrary::addPattern (const juce::String& name, int numBars)
{
    const auto sig = juce::jmax (1, (int) state.transport().getProperty (IDs::timeSigNum, 4));

    juce::ValueTree tree (IDs::pattern);
    tree.setProperty (IDs::uid,         allocateUid(), nullptr);
    tree.setProperty (IDs::name,        name.isNotEmpty() ? name : uniqueName (patterns(), kDefaultPatternName), nullptr);
    tree.setProperty (IDs::patternLen,  (double) juce::jmax (1, numBars) * sig, nullptr);

    state.addChild (patterns(), tree, -1, "Add pattern");
    setActivePatternUid (uidOf (tree));

    return patterns().getChildWithProperty (IDs::uid, tree.getProperty (IDs::uid));
}

juce::ValueTree PatternLibrary::duplicatePattern (const juce::ValueTree& pattern)
{
    if (! pattern.isValid())
        return {};

    auto copy = pattern.createCopy();
    copy.setProperty (IDs::uid,  allocateUid(), nullptr);
    copy.setProperty (IDs::name, uniqueName (patterns(),
                                              pattern.getProperty (IDs::name).toString() + " copy"), nullptr);

    state.addChild (patterns(), copy, -1, "Duplicate pattern");

    return patterns().getChildWithProperty (IDs::uid, copy.getProperty (IDs::uid));
}

bool PatternLibrary::deletePattern (const juce::ValueTree& pattern)
{
    if (! pattern.isValid() || pattern.getParent() != patterns())
        return false;

    if (patternInUse (uidOf (pattern)))
        return false;

    state.removeChild (patterns(), pattern, "Delete pattern");

    if (activePatternUid() == uidOf (pattern))
        setActivePatternUid (patternCount() > 0 ? uidOf (patterns().getChild (0)) : -1);

    return true;
}

juce::ValueTree PatternLibrary::patternForUid (int uid) const
{
    if (uid < 0)
        return {};

    return patterns().getChildWithProperty (IDs::uid, uid);
}

int PatternLibrary::uidOf (const juce::ValueTree& pattern) const
{
    return pattern.isValid() ? (int) pattern.getProperty (IDs::uid, -1) : -1;
}

bool PatternLibrary::patternInUse (int uid) const
{
    if (uid < 0)
        return false;

    auto all = tracks();
    for (int i = 0; i < all.getNumChildren(); ++i)
    {
        auto track = all.getChild (i);
        for (int c = 0; c < track.getNumChildren(); ++c)
        {
            auto clip = track.getChild (c);
            if (clip.getType() == juce::Identifier (IDs::clip)
                && (int) clip.getProperty (IDs::patternUid, -1) == uid)
                return true;
        }
    }
    return false;
}

double PatternLibrary::patternLengthBeats (const juce::ValueTree& pattern) const
{
    const auto sig = juce::jmax (1, (int) state.transport().getProperty (IDs::timeSigNum, 4));
    return (double) juce::jmax (1, (int) pattern.getProperty (IDs::patternLen, sig * 4));
}

int PatternLibrary::activePatternUid() const
{
    return (int) state.root.getProperty (IDs::activePattern, -1);
}

juce::ValueTree PatternLibrary::activePattern() const
{
    return patternForUid (activePatternUid());
}

void PatternLibrary::setActivePatternUid (int uid)
{
    state.root.setProperty (IDs::activePattern, uid, nullptr);
}

juce::ValueTree PatternLibrary::addNote (const juce::ValueTree& pattern, int pitch, double pos,
                                         double len, double vel, bool newTransaction)
{
    if (! pattern.isValid())
        return {};

    if (noteCount (pattern) >= kMaxNotes)
        return {};

    juce::ValueTree note (IDs::note);
    note.setProperty (IDs::pitch,    juce::jlimit (0, 127, pitch), nullptr);
    note.setProperty (IDs::start,    juce::jmax (0.0, pos), nullptr);
    note.setProperty (IDs::duration, juce::jmax (0.03125, len), nullptr);
    note.setProperty (IDs::velocity, juce::jlimit (0.0, 1.0, vel), nullptr);

    state.addChild (pattern, note, -1, "Add note", newTransaction);

    return pattern.getChild (pattern.getNumChildren() - 1);
}

bool PatternLibrary::deleteNote (const juce::ValueTree& pattern, const juce::ValueTree& note)
{
    if (! pattern.isValid() || ! note.isValid() || note.getParent() != pattern)
        return false;

    state.removeChild (pattern, note, "Delete note");
    return true;
}

juce::ValueTree PatternLibrary::noteAt (const juce::ValueTree& pattern, int pitch, double pos) const
{
    for (int i = pattern.getNumChildren() - 1; i >= 0; --i)
    {
        auto n = pattern.getChild (i);
        if (n.getType() != juce::Identifier (IDs::note))
            continue;

        if ((int) n.getProperty (IDs::pitch) != pitch)
            continue;

        const auto s = (double) n.getProperty (IDs::start);
        const auto d = (double) n.getProperty (IDs::duration);

        if (pos >= s && pos < s + d)
            return n;
    }
    return {};
}

int PatternLibrary::noteCount (const juce::ValueTree& pattern) const
{
    int n = 0;
    for (int i = 0; i < pattern.getNumChildren(); ++i)
        if (pattern.getChild (i).getType() == juce::Identifier (IDs::note))
            ++n;
    return n;
}

juce::ValueTree PatternLibrary::addClip (const juce::ValueTree& track, int uid, double start, double length)
{
    if (! track.isValid() || patternForUid (uid).isValid() == false)
        return {};

    juce::ValueTree clip (IDs::clip);
    clip.setProperty (IDs::uid,        allocateUid(), nullptr);
    clip.setProperty (IDs::patternUid, uid, nullptr);
    clip.setProperty (IDs::start,      juce::jmax (0.0, start), nullptr);
    clip.setProperty (IDs::duration,   juce::jmax (0.25, length), nullptr);

    state.addChild (track, clip, -1, "Add clip");

    return track.getChildWithProperty (IDs::uid, clip.getProperty (IDs::uid));
}

bool PatternLibrary::deleteClip (const juce::ValueTree& track, const juce::ValueTree& clip)
{
    if (! track.isValid() || ! clip.isValid() || clip.getParent() != track)
        return false;

    state.removeChild (track, clip, "Delete clip");
    return true;
}

bool PatternLibrary::makeUnique (const juce::ValueTree& clip)
{
    if (! clip.isValid() || clip.getType() != juce::Identifier (IDs::clip))
        return false;

    const auto original = patternForUid ((int) clip.getProperty (IDs::patternUid, -1));
    if (! original.isValid())
        return false;

    auto uniquePattern = duplicatePattern (original);
    if (! uniquePattern.isValid())
        return false;

    state.setProperty (clip, IDs::patternUid, uidOf (uniquePattern), "Make unique");
    return true;
}

int PatternLibrary::clipCount() const
{
    int n = 0;
    auto all = tracks();
    for (int i = 0; i < all.getNumChildren(); ++i)
    {
        auto track = all.getChild (i);
        for (int c = 0; c < track.getNumChildren(); ++c)
            if (track.getChild (c).getType() == juce::Identifier (IDs::clip))
                ++n;
    }
    return n;
}

juce::ValueTree PatternLibrary::clipForUid (int uid) const
{
    auto all = tracks();
    for (int i = 0; i < all.getNumChildren(); ++i)
    {
        auto track = all.getChild (i);
        auto clip = track.getChildWithProperty (IDs::uid, uid);
        if (clip.isValid() && clip.getType() == juce::Identifier (IDs::clip))
            return clip;
    }
    return {};
}

int PatternLibrary::allocateUid()
{
    auto& root = state.root;
    const int next = (int) root.getProperty (IDs::uid, 0) + 1;
    root.setProperty (IDs::uid, next, nullptr);
    return next;
}

juce::String PatternLibrary::uniqueName (const juce::ValueTree& patternsTree, const juce::String& base)
{
    for (int n = 1; n < 10000; ++n)
    {
        const auto candidate = (n == 1 ? base : base + " " + juce::String (n));

        bool found = false;
        for (int i = 0; i < patternsTree.getNumChildren(); ++i)
            if (patternsTree.getChild (i).getProperty (IDs::name).toString() == candidate)
            { found = true; break; }

        if (! found)
            return candidate;
    }

    return base;
}
