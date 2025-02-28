#ifndef FPS_H_INCLUDED
#define FPS_H_INCLUDED

#include <chrono>
#include <vector>

class FpsCounter
{
  public:
    FpsCounter(float update_interval)
        : fps(0.0f)
        , update_interval(update_interval)
    {
        restart();
    }

    void restart()
    {
        start_time = std::chrono::high_resolution_clock::now();
        last_frame = start_time;
        frame_times.clear();
    }

    void add_frame()
    {
        auto this_frame = std::chrono::high_resolution_clock::now();
        frame_times.push_back(std::chrono::duration<float>(this_frame - last_frame).count());
        last_frame = this_frame;

        if (std::chrono::duration<float>(this_frame - start_time).count() >= update_interval) {
            float total_time = 0.0f;
            for (float frame_time : frame_times)
                total_time += frame_time;

            fps = frame_times.size() / total_time;

            frame_times.clear();
            start_time = this_frame;
        }
    }

    inline float frames_per_second() { return fps; }

  private:
    float fps;
    const float update_interval;
    std::vector<float> frame_times;
    std::chrono::high_resolution_clock::time_point start_time, last_frame;
};

#endif