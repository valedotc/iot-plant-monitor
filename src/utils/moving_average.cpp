#include "moving_average.h"

namespace PlantMonitor {
namespace Utils {   
MovingAverage::MovingAverage(size_t size)
    : m_size(size) {
}
void MovingAverage::addSample(float sample) {
    if (m_samples.size() >= m_size) {
        m_samples.erase(m_samples.begin());
    }
    m_samples.push_back(sample);
}
float MovingAverage::getAverage() const {
    if (m_samples.empty()) {
        return 0.0f;
    }
    float sum = std::accumulate(m_samples.begin(), m_samples.end(), 0.0f);
    return sum / m_samples.size();
}
void MovingAverage::clear() {
    m_samples.clear();
}
} // namespace Utils
} // namespace PlantMonitor