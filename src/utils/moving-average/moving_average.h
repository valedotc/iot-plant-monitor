#pragma once
#include <vector>
#include <numeric>

/*!
 * \file moving_average.h
 * \brief Simple moving average filter utility
 */


namespace PlantMonitor {
namespace Utils {
/*!
 * \class MovingAverage
 * \brief Implements a simple moving average filter
 */
class MovingAverage {
public:
    /*! 
     * \brief Constructor
     * \param size Number of samples to average
     */
    explicit MovingAverage(size_t size);

    /*!
     * \brief Add a new sample to the filter
     * \param sample New sample value
     */
    void addSample(float sample);

    /*!
     * \brief Get the current average value
     * \return Average of the samples
     */
    float getAverage() const;

    /*!
     * \brief Clear all stored samples
     */
    void clear();
private:
    size_t m_size;                   //!< Number of samples to average
    std::vector<float> m_samples; //!< Stored samples
    size_t m_index;                  //!< Current index for circular buffer
    size_t m_count;                  //!< Number of samples added
};
} // namespace Utils
} // namespace PlantMonitor
