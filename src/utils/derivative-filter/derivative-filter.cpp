#include "derivative-filter.h"

namespace PlantMonitor {
namespace Utils {

DerivativeFilter::DerivativeFilter(float scale, size_t smooth_window)
    : m_previous_input(0.0f),
      m_has_previous(false),
      m_scale(scale),
      m_smoother(nullptr) {
    if (smooth_window > 0) {
        m_smoother = new MovingAverage(smooth_window);
    }
}

DerivativeFilter::~DerivativeFilter() {
    delete m_smoother;
}

float DerivativeFilter::apply(float input) {
    
    float processed_input = input;
    if (m_smoother) {
        m_smoother->addSample(input);
        processed_input = m_smoother->getAverage();
    }
    
    float derivative = 0.0f;
    if (m_has_previous) {
        derivative = (processed_input - m_previous_input) * m_scale;
    } else {
        m_has_previous = true;
    }
    
    m_previous_input = processed_input;
    return derivative;
}

void DerivativeFilter::reset() {
    m_has_previous = false;
    m_previous_input = 0.0f;
    if (m_smoother) {
        m_smoother->clear();
    }
}

} // namespace Utils
} // namespace PlantMonitor