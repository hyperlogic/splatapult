#include "sortbuddy.h"

#include <algorithm>

#include "core/log.h"

static void SortThreadFunc(SortBuddy* sortBuddy)
{

}

SortBuddy::SortBuddy(std::shared_ptr<GaussianCloud> gaussianCloud) : cameraSignal(0)
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
        //
        // NOTE: This code runs on sort thread!
        //

        bool done = false;  // AJT: TODO fix me!

        sortId++;

        while (!done)
        {
            cameraSignal.acquire();
            glm::mat4 cameraMat = lockedCameraMat.Get();
            glm::vec3 cameraPos = glm::vec3(cameraMat[3]);
            glm::vec3 forward = glm::normalize(glm::vec3(cameraMat[2]));

            using IndexDistPair = std::pair<uint32_t, float>;
            std::vector<IndexDistPair> indexDistVec;
            indexDistVec.reserve(numPoints);
            for (uint32_t i = 0; i < numPoints; i++)
            {
                indexDistVec.push_back(IndexDistPair(i, glm::dot(forward, posVec[i] - cameraPos)));
            }

            std::sort(indexDistVec.begin(), indexDistVec.end(), [](const IndexDistPair& a, const IndexDistPair& b)
            {
                return a.second < b.second;
            });

            // copy result
            lockedIndexVec.WithLock([&indexDistVec](std::vector<uint32_t>& indexVec)
            {
                assert(indexVec.size() == indexDistVec.size());
                for (size_t i = 0; i < indexVec.size(); i++)
                {
                    indexVec[i] = indexDistVec[i].first;
                }
            });

            sortId++;
        }
    });
    sortThread->detach();
}

SortBuddy::~SortBuddy()
{
    // TODO:
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
