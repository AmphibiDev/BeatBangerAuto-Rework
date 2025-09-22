#include "patternmatcher.h"

PatternMatcher::PatternMatcher(const std::vector<int>& pattern)
    : m_pattern(pattern), m_patternSize(pattern.size())
{
    buildBadCharTable();
}

void PatternMatcher::buildBadCharTable()
{
    m_badCharTable.fill(m_patternSize);

    for (size_t i = 0; i < m_patternSize - 1; ++i) {
        if (m_pattern[i] != -1) {
            uint8_t byte = static_cast<uint8_t>(m_pattern[i]);
            m_badCharTable[byte] = m_patternSize - 1 - i;
        } else {
            for (size_t j = 0; j < 256; ++j) {
                if (m_badCharTable[j] > m_patternSize - 1 - i) {
                    m_badCharTable[j] = m_patternSize - 1 - i;
                }
            }
        }
    }
}

size_t PatternMatcher::find(const uint8_t* data, size_t dataSize) const
{
    if (m_patternSize == 0 || dataSize < m_patternSize) {
        return SIZE_MAX;
    }

    size_t pos = 0;
    while (pos <= dataSize - m_patternSize) {
        size_t patternPos = m_patternSize - 1;

        while (true) {
            uint8_t dataByte = data[pos + patternPos];
            int patternByte = m_pattern[patternPos];

            if (patternByte != -1 && dataByte != static_cast<uint8_t>(patternByte)) {
                pos += m_badCharTable[data[pos + m_patternSize - 1]];
                break;
            }

            if (patternPos == 0) {
                return pos;
            }
            --patternPos;
        }
    }

    return SIZE_MAX;
}
