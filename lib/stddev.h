
#include <cmath>

template <class T>
T calc_stddev(const T *array, int array_size)
{
    if (array == nullptr || array_size <= 0)
        return 0.0;

    // Calculate the mean (average)
    T sum = 0.0;
    for (int i = 0; i < array_size; ++i)
        sum += array[i];
    T mean = sum / array_size;

    // Step 2: Calculate the sum of squared differences from the mean
    T squared_diff_sum = 0.0;
    for (int i = 0; i < array_size; ++i)
    {
        T diff = array[i] - mean;
        squared_diff_sum += diff * diff;
    }

    // Step 3: Calculate variance and standard deviation
    T variance = squared_diff_sum / array_size;

    return std::sqrt(variance);
}
