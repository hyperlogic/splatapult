#include "sortbuddy.h"

#include <algorithm>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/log.h"

using KeyType = uint32_t;
static void RadixSort(std::vector<KeyType>& keyVec, std::vector<uint32_t>& valVec, bool decending = false)
{
    std::vector<KeyType> keyVec2 = keyVec;
    std::vector<uint32_t> valVec2 = valVec;
    assert(keyVec.size() == valVec.size());
    const uint32_t N = 4;
    const uint32_t TABLE_SIZE = 1 << N;
    uint32_t mask = (1 << N) - 1;
    uint32_t shift = 0;
    for (size_t i = 0; i < (32 / N); i++)
    {
        size_t counts[TABLE_SIZE] = {};

        // count
        for (size_t j = 0; j < keyVec.size(); j++)
        {
            uint32_t key = ((keyVec[j] >> shift) & mask);
            counts[key]++;
        }

        size_t offsets[TABLE_SIZE] = {};
        memset(offsets, 0, sizeof(size_t) * TABLE_SIZE);
        size_t accum = 0;
        if (decending)
        {
            // suffix scan
            for (size_t j = TABLE_SIZE - 1; j != std::numeric_limits<size_t>::max(); j--)
            {
                offsets[j] = accum;
                accum += counts[j];
            }
        }
        else
        {
            // prefix scan
            for (size_t j = 0; j < TABLE_SIZE; j++)
            {
                offsets[j] = accum;
                accum += counts[j];
            }
        }

        // reorder
        for (size_t j = 0; j < keyVec.size(); j++)
        {
            uint32_t key = ((keyVec[j] >> shift) & mask);
            size_t ii = offsets[key];
            keyVec2[ii] = keyVec[j];
            valVec2[ii] = valVec[j];
            offsets[key]++;
        }

        keyVec.swap(keyVec2);
        valVec.swap(valVec2);

        shift += N;
    }

    keyVec.swap(keyVec2);
    valVec.swap(valVec2);
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

        std::vector<KeyType> keyVec;
        keyVec.reserve(numPoints);
        std::vector<uint32_t> valVec;
        valVec.reserve(numPoints);
        for (uint32_t i = 0; i < numPoints; i++)
        {
            keyVec.push_back(0);
            valVec.push_back(i);
        }

        sortId++;
        while (!sortThreadShouldQuit)
        {
#ifdef TRACY_ENABLE
            FrameMark;
#endif
            {
                ZoneScopedNC("sort signal", tracy::Color::Red4);
                cameraSignal.acquire();
            }

            {
                ZoneScopedNC("sort pre-sort", tracy::Color::Green);

                glm::mat4 cameraMat = lockedCameraMat.Get();
                glm::vec3 cameraPos = glm::vec3(cameraMat[3]);
                glm::vec3 forward = -glm::normalize(glm::vec3(cameraMat[2]));

                keyVec.clear();
                valVec.clear();

                const float Z_FAR = 1000.0f;
                const float KEY_MAX = static_cast<float>(std::numeric_limits<KeyType>::max());
                for (uint32_t i = 0; i < numPoints; i++)
                {
                    float z = glm::dot(forward, posVec[i] - cameraPos);
                    KeyType key;
                    if (z > 0)
                    {
                        key = static_cast<KeyType>((z / Z_FAR) * KEY_MAX);
                    }
                    else
                    {
                        key = 0;
                    }
                    keyVec.push_back(key);
                    valVec.push_back(i);
                }
            }

            {
                ZoneScopedNC("sort", tracy::Color::Blue);

                /*
                std::sort(valVec.begin(), valVec.end(), [&keyVec](const uint32_t& a, const uint32_t& b)
                {
                    return keyVec[a] > keyVec[b];
                });
                */

                RadixSort(keyVec, valVec, true);
            }

            // copy result
            {
                ZoneScopedNC("sort copy", tracy::Color::Red4);
                lockedIndexVec.WithLock([&valVec, &keyVec](std::vector<uint32_t>& indexVec)
                {
                    indexVec.swap(valVec);
                });
            }

            sortId++;
        }
    });

    // AJT: TODO REMOVE: TEST
    std::vector<KeyType> keyVec = {0, 3, 2, 2, 3, 2, 0, 3, 2, 1};
    std::vector<uint32_t> valVec = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    printf("keyVec = [");
    for (size_t i = 0; i < keyVec.size(); i++)
    {
        printf("%u", keyVec[i]);
        if (i != keyVec.size() - 1)
        {
            printf(",");
        }
    }
    printf("]\n");

    printf("valVec = [");
    for (size_t i = 0; i < valVec.size(); i++)
    {
        printf("%u", valVec[i]);
        if (i != valVec.size() - 1)
        {
            printf(",");
        }
    }
    printf("]\n");

    RadixSort(keyVec, valVec, true);

    printf("keyVec (sorted) = [");
    for (size_t i = 0; i < keyVec.size(); i++)
    {
        printf("%u", keyVec[i]);
        if (i != keyVec.size() - 1)
        {
            printf(",");
        }
    }
    printf("]\n");

    printf("valVec (sorted) = [");
    for (size_t i = 0; i < valVec.size(); i++)
    {
        printf("%u", valVec[i]);
        if (i != valVec.size() - 1)
        {
            printf(",");
        }
    }
    printf("]\n");
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
