#pragma once
#include "utils/moving-average/moving-average.h"

/*!
 * \file derivative_filter.h
 * \brief Derivative filter with optional input smoothing
 */

#define DERIVATIVE_FILTER_DEFAULT_SCALE (1.0f) //!< Default scaling factor for derivative filter
#define DERIVATIVE_FILTER_DEFAULT_SMOOTH_WINDOW (0u) //!< Default moving average window size (0 = no smoothing)

namespace PlantMonitor {
namespace Utils {
/*!
* \class DerivativeFilter
* \brief Implements a derivative filter with optional moving average smoothing
*/
class DerivativeFilter {
public:
    /*!
     * \brief Constructor
     * \param scale Scaling factor for the derivative (typically 1/dt)
     * \param smooth_window Size of moving average window (0 = no smoothing)
     */
    explicit DerivativeFilter(float scale = DERIVATIVE_FILTER_DEFAULT_SCALE, size_t smooth_window = DERIVATIVE_FILTER_DEFAULT_SMOOTH_WINDOW);

    /*!
     * \brief Destructor
     */
    ~DerivativeFilter();
    
    /*!
     * \brief Apply the derivative filter to a new sample
     * \param input New sample value
     * \return Derivative value (rate of change)
     */
    float apply(float input);
    
    /*!
     * \brief Reset the filter state
     */
    void reset();

private:
    float m_previous_input;        //!< Previous input sample
    bool m_has_previous;           //!< Flag indicating if previous input exists
    float m_scale;                 //!< Scaling factor for the derivative
    MovingAverage* m_smoother;     //!< Optional input smoother (nullptr if disabled)
};
} // namespace Utils
} // namespace PlantMonitor