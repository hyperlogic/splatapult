#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "core/threadsafevalue.h"
#include "gaussiancloud.h"

class SortBuddy
{
    friend void SortThreadFunc(SortBuddy* sortBuddy);
public:
    SortBuddy(std::shared_ptr<GaussianCloud> gaussianCloud);
    ~SortBuddy();

    // thread-safe
    void SetLatestCamera(const glm::mat4& cameraMat);

    // thread-safe - callback will be invoked on the callers thread.
    using GetSortCallback = std::function<const void(const std::vector<uint32_t>&, uint32_t)>;
    void GetSortWithLock(const GetSortCallback& cb);

protected:
    std::vector<glm::vec3> posVec;
    ThreadSafeValue<std::vector<uint32_t>> lockedIndexVec;
    std::atomic<uint32_t> sortId;
    std::atomic<bool> sortThreadShouldQuit;
    std::shared_ptr<std::thread> sortThread;
    std::binary_semaphore cameraSignal;

    ThreadSafeValue<glm::mat4> lockedCameraMat;
};
