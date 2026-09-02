
#include <cmath>

template <class T>
T calc_variance(const T *array, int array_size)
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

    return variance;
}

template <class T>
T calc_stddev(const T *array, int array_size)
{
    return std::sqrt(calc_variance(array,array_size));
}

template <class T, int hist_size>
class stats_history
{
    T  history[hist_size];
    int pos;
    int got;
public:
    stats_history(void) {
        reset();
    }
    void reset(void) {
        for (int ind = 0; ind < hist_size; ind++)
            history[ind] = 0;
        pos = got = 0;
    }
    void add(T v) {
        history[pos] = v;
        if (++pos >= hist_size)
            pos = 0;
        if (got < hist_size)
            got++;
    }
    int count(void) const { return got; }
    T variance(void) {
        return calc_variance(history, got);
    }
    T stddev(void) {
        return calc_stddev(history, got);
    }
    T average(void) {
        if (got == 0)
            return 0;
        T sum = 0;
        for (int ind = 0; ind < got; ind++)
            sum += history[ind];
        return sum / got;
    }
};
