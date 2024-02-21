#include "sortbuddy.h"

#include <algorithm>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/log.h"

static void SortThreadFunc(SortBuddy* sortBuddy)
{

}

SortBuddy::SortBuddy(std::shared_ptr<GaussianCloud> gaussianCloud) : cameraSignal(0), sortThreadShouldQuit(false)
{
    size_t numPoints = gaussianCloud->size();
    assert(numPoints <= std::numeric_limits<uint32_t>::max());

    posVec.reserve(numPoints);
    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        posVec.emplace_back(glm::vec3(g.position[0], g.position[1], g.position[2]));
    }

    lockedIndexVec.WithLock([this, numPoints](std::vector<uint32_t>& indexVec)
    {
        indexVec.reserve(numPoints);
        for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
        {
            indexVec.push_back(i);
        }
    });

    sortId = 1;

    sortThread = std::make_shared<std::thread>([this, numPoints]()
    {
#ifdef TRACY_ENABLE
        tracy::SetThreadName("sort thread");
#endif

        sortId++;

        while (!sortThreadShouldQuit)
        {
            FrameMark;

            {
                ZoneScopedNC("sort signal", tracy::Color::Red4);
                cameraSignal.acquire();
            }

            using IndexDistPair = std::pair<uint32_t, float>;
            std::vector<IndexDistPair> indexDistVec;

            {
                ZoneScopedNC("sort pre-sort", tracy::Color::Green);

                glm::mat4 cameraMat = lockedCameraMat.Get();
                glm::vec3 cameraPos = glm::vec3(cameraMat[3]);
                glm::vec3 forward = glm::normalize(glm::vec3(cameraMat[2]));


                indexDistVec.reserve(numPoints);
                for (uint32_t i = 0; i < numPoints; i++)
                {
                    indexDistVec.push_back(IndexDistPair(i, glm::dot(forward, posVec[i] - cameraPos)));
                }
            }

            {
                ZoneScopedNC("sort", tracy::Color::Blue);
                std::sort(indexDistVec.begin(), indexDistVec.end(), [](const IndexDistPair& a, const IndexDistPair& b)
                {
                    return a.second < b.second;
                });
            }

            // copy result
            {
                ZoneScopedNC("sort copy", tracy::Color::Red4);
                lockedIndexVec.WithLock([&indexDistVec](std::vector<uint32_t>& indexVec)
                {
                    ZoneScopedNC("sort copy body", tracy::Color::Green);
                    assert(indexVec.size() == indexDistVec.size());
                    for (size_t i = 0; i < indexVec.size(); i++)
                    {
                        indexVec[i] = indexDistVec[i].first;
                    }
                });
            }

            sortId++;
        }
    });
}

SortBuddy::~SortBuddy()
{
    sortThreadShouldQuit = true;
    sortThread->join();
}

// thread-safe
void SortBuddy::SetLatestCamera(const glm::mat4& cameraMat)
{
    lockedCameraMat.Set(cameraMat);
    cameraSignal.release();
}

// thread-safe - callback will be invoked on the callers thread.
void SortBuddy::GetSortWithLock(const GetSortCallback& cb)
{
    lockedIndexVec.WithLock([this, &cb](const std::vector<uint32_t>& indexVec)
    {
        cb(indexVec, sortId);
    });
}
