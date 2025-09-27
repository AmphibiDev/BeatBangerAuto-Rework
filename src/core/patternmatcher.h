#ifndef PATTERNMATCHER_H
#define PATTERNMATCHER_H

// STL includes
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

class PatternMatcher
{
public:
    explicit PatternMatcher(const std::vector<int>& pattern);

    size_t search(const uint8_t* data, size_t dataSize) const;
    size_t getPatternSize() const { return m_patternSize; }
    bool isValid() const { return m_patternSize > 0; }

private:
    void buildBadCharTable();

    std::vector<int> m_pattern;
    std::array<size_t, 256> m_badCharTable;
    size_t m_patternSize;
};

#endif // PATTERNMATCHER_H
