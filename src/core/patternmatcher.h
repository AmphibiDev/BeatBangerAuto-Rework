#ifndef PATTERNMATCHER_H
#define PATTERNMATCHER_H

#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

class PatternMatcher
{
public:
    explicit PatternMatcher(const std::vector<int>& pattern);

    size_t find(const uint8_t* data, size_t dataSize) const;
    size_t getPatternSize() const { return m_patternSize; }
    bool isValid() const { return m_patternSize > 0; }

private:
    std::vector<int> m_pattern;
    std::array<size_t, 256> m_badCharTable;
    size_t m_patternSize;

    void buildBadCharTable();
};

#endif // PATTERNMATCHER_H
