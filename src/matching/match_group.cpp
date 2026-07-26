#include "match_group.h"
#include <sstream>
#include <string>

/// Reserved upper bound on model count; not currently referenced in this file.
#define MAX_NUM_MODEL (int)1000

/// Vision/matching module: MatchGroupConfig and MatchGroup implementation
/// (see match_group.h).
namespace mtc {

int MatchGroup::m_min_index_range = 1;
int MatchGroup::m_max_index_range = 32;
int MatchGroup::m_min_pattern_range = 1;
int MatchGroup::m_max_pattern_range = 4;

// ── MatchGroupConfig — deep-copy of the shared typeConfig ──────────────────────

MatchGroupConfig::MatchGroupConfig()
    : typeConfig(std::make_shared<EdgeMatchConfig>()) {}

MatchGroupConfig::MatchGroupConfig(const MatchGroupConfig &other)
    : m_groupName(other.m_groupName),
      m_groupIndex(other.m_groupIndex),
      m_sortByAngle(other.m_sortByAngle),
      m_sortConditionAngle(other.m_sortConditionAngle),
      typeConfig(other.typeConfig ? other.typeConfig->clone() : nullptr),
      m_patterns(other.m_patterns) {}

MatchGroupConfig &MatchGroupConfig::operator=(const MatchGroupConfig &other) {
    if (this != &other) {
        m_groupName          = other.m_groupName;
        m_groupIndex         = other.m_groupIndex;
        m_sortByAngle        = other.m_sortByAngle;
        m_sortConditionAngle = other.m_sortConditionAngle;
        typeConfig           = other.typeConfig ? other.typeConfig->clone() : nullptr;
        m_patterns           = other.m_patterns;
    }
    return *this;
}

MatchGroup::MatchGroup() {

}

MatchGroup::MatchGroup(std::wstring name, int index)
    : m_manager{nullptr} {
    m_config.m_groupName = name;
    m_config.m_groupIndex = index;

}

MatchGroup::~MatchGroup() {

}

void MatchGroup::cloneConfigTo(MatchGroup &group) {
    group.m_config = this->m_config;
}

MatchGroup::MatchGroup(const MatchGroupConfig &config,
                           PatternGroupManager *parent)
    :  m_manager(parent), m_config(config) {

}

std::vector<std::shared_ptr<MatchPattern>> MatchGroup::patterns() const {
    return m_patterns;
}

MatchPattern* MatchGroup::findPatternByName(const std::wstring &name) const {
    for (std::shared_ptr<MatchPattern> p : m_patterns)
        if (p->name() == name) return p.get();
    return nullptr;
}

MatchPattern* MatchGroup::findPatternByNumber(int number) const {
    for (std::shared_ptr<MatchPattern> p : m_patterns)
        if (p->number() == number) return p.get();
    return nullptr;
}

bool MatchGroup::containsPatternName(const std::wstring &name) const
{
    return findPatternByName(name) != nullptr;
}

bool MatchGroup::containsPatternNumber(int number) const
{
    return findPatternByNumber(number) != nullptr;
}

int MatchGroup::patternCount() const {
    return m_patterns.size();
}

bool MatchGroup::isEmpty() const {
    return (m_patterns.size() <= 0);
}

// ── Pattern mutations ─────────────────────────────────────────────────────────

ManagerResult MatchGroup::addPattern(const MatchPatternConfig &config) {
    if (auto r = validatePatternConfig(config); !r)
        return r;

    // MatchPattern* pattern = new MatchPattern(config);
    std::shared_ptr<MatchPattern> pattern = std::make_shared<MatchPattern>(config, this);
    m_patterns.push_back(pattern);
    m_lastPatternAccess = pattern;
    return ManagerResult::success();
}

ManagerResult MatchGroup::removePattern(const std::wstring &patternName) {
    MatchPattern *pattern = findPatternByName(patternName);

    if (!pattern) {
        std::wstringstream wss;
        wss << L"Pattern \"" << patternName << L"\" not found in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }

    auto it = m_patterns.cbegin();
    while (it != m_patterns.cend()) {
        if (it._Ptr->get() == pattern) {
            m_patterns.erase(it);
            break;
        }
        it++;
    }

    return ManagerResult::success();
}

ManagerResult MatchGroup::removePatternByNumber(int number) {
    MatchPattern *pattern = findPatternByNumber(number);

    if (!pattern) {
        std::wstringstream wss;
        wss << L"No pattern with number " << number << L" in group " << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }
    return removePattern(pattern->name());
}

std::shared_ptr<MatchPattern> MatchGroup::lastPatternAccess() const {
    return m_lastPatternAccess;
}

ManagerResult MatchGroup::setPatternConfig(const std::wstring &currentName,
                                             const MatchPatternConfig &newConfig) {
    MatchPattern *pattern = findPatternByName(currentName);
    if (!pattern) {
        std::wstringstream wss;
        wss << L"Pattern \"" << currentName << L"\" not found in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }

    // Validate — exclude the pattern itself from the duplicate check.
    if (auto r = validatePatternConfig(newConfig, currentName); !r)
        return r;

    return pattern->setConfig(newConfig);
}

ManagerResult MatchGroup::renamePattern(const std::wstring &currentName,
                                          const std::wstring &newName) {
    if (currentName == newName)
        return ManagerResult::success();

    if (!containsPatternName(newName)) {
        std::wstringstream wss;
        wss << L"A pattern named \"" << newName << L"\" already exists in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }

    MatchPattern *pattern = findPatternByName(currentName);
    if (!pattern) {
        std::wstringstream wss;
        wss << L"Pattern \"" << currentName << L"\" not found in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }
    return pattern->setName(newName);
}

ManagerResult MatchGroup::renumberPattern(const std::wstring &patternName,
                                            int newNumber) {
    MatchPattern *pattern = findPatternByName(patternName);
    if (!pattern) {
        std::wstringstream wss;
        wss << L"Pattern \"" << patternName << L"\" not found in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }

    if (pattern->number() == newNumber)
        return ManagerResult::success();

    if (containsPatternNumber(newNumber)) {
        std::wstringstream wss;
        wss << L"Pattern number\"" << newNumber << L"\" already exists in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }

    return pattern->setNumber(newNumber);
}

ManagerResult MatchGroup::setPatternImage(const std::wstring &patternName,
                                            const cv::Mat &image) {
    MatchPattern *pattern = findPatternByName(patternName);
    if (!pattern) {
        std::wstringstream wss;
        wss << L"Pattern \"" << patternName << L"\" not found in group \"" << m_config.m_groupName << L"\".";
        return ManagerResult::fail(wss.str());
    }
    pattern->setImage(image);
    return ManagerResult::success();
}

// ── Private (called only by PatternGroupManager) ──────────────────────────────

ManagerResult MatchGroup::setConfig(const MatchGroupConfig &cfg) {
    // MatchGroup is not a QObject — change notification happens at the
    // manager level (PatternGroupManager emits groupChanged after this
    // succeeds).
    m_config = cfg;
    return ManagerResult::success();
}

ManagerResult MatchGroup::setName(const std::wstring &name) {
    if (m_config.m_groupName == name) return ManagerResult::success();
    m_config.m_groupName = name;
    // emit configChanged(m_config, QStringLiteral("name"));
    return ManagerResult::success();
}

ManagerResult MatchGroup::setNumber(int number) {
    if (m_config.m_groupIndex == number) return ManagerResult::success();
    m_config.m_groupIndex = number;
    // emit configChanged(m_config, QStringLiteral("number"));
    return ManagerResult::success();
}

// ── Validation ────────────────────────────────────────────────────────────────

ManagerResult MatchGroup::validatePatternConfig(const MatchPatternConfig &cfg,
                                                  const std::wstring &excludeName) const
{
    if (cfg.m_patternName.empty())
        return ManagerResult::fail(L"Pattern name must not be empty.");

    for (std::shared_ptr<MatchPattern> p : m_patterns) {
        if (p->name() == excludeName) {
            continue;   // skip self
        }

        if (p->name() == cfg.m_patternName) {
            std::wstringstream wss;
            wss << L"A pattern named \"" << cfg.m_patternName << L"\" already exists in group \"" << m_config.m_groupName << L"\".";
            return ManagerResult::fail(wss.str());
        }

        if (p->number() == cfg.m_patternIndex) {
            std::wstringstream wss;
            wss << L"Pattern number\"" << cfg.m_patternIndex << L"\" already exists in group \"" << m_config.m_groupName << L"\".";
            return ManagerResult::fail(wss.str());
        }
    }
    return ManagerResult::success();
}

bool MatchGroup::validateIndexRange(int index) {
    return ((index >= m_min_index_range) && (index <= m_max_index_range));
}

bool MatchGroup::setIndexRange(int min, int max) {
    if ((min >= 0) && (max > min)) {
        m_min_index_range = min;
        m_max_index_range = max;
        return true;
    }
    return false;
}

int MatchGroup::getMaxGroupRange() {
    return m_max_index_range;
}

int MatchGroup::getMinGroupRange() {
    return m_min_index_range;
}

int MatchGroup::getMaxPatternRange() {
    return m_max_pattern_range;
}

int MatchGroup::getMinPatternRange() {
    return m_min_pattern_range;
}


} // end namespace mtc
