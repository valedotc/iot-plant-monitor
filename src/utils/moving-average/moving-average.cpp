#include "moving-average.h"
#include <numeric>

namespace PlantMonitor {
namespace Utils {

MovingAverage::MovingAverage(size_t size)
    : m_size(size), m_index(0), m_count(0) {
    m_samples.resize(size, 0.0f);
}

void MovingAverage::addSample(float sample) {
    m_samples[m_index] = sample;
    m_index = (m_index + 1) % m_size;
    if (m_count < m_size) {
        m_count++;
    }
}

float MovingAverage::getAverage() const {
    if (m_count == 0) {
        return 0.0f;
    }
    float sum = std::accumulate(m_samples.begin(), 
                                 m_samples.begin() + m_count, 
                                 0.0f);
    return sum / m_count;
}

void MovingAverage::clear() {
    m_index = 0;
    m_count = 0;
    std::fill(m_samples.begin(), m_samples.end(), 0.0f);
}

} // namespace Utils
} // namespace PlantMonitor