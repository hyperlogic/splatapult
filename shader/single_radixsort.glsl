/**
* VkRadixSort written by Mirco Werner: https://github.com/MircoWerner/VkRadixSort
* Based on implementation of Intel's Embree: https://github.com/embree/embree/blob/v4.0.0-ploc/kernels/rthwif/builder/gpu/sort.h
*/
#version 460
#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

#define WORKGROUP_SIZE 256// assert WORKGROUP_SIZE >= RADIX_SORT_BINS
#define RADIX_SORT_BINS 256
#define SUBGROUP_SIZE 32// 32 NVIDIA; 64 AMD

#define BITS 32// sorting uint32_t
#define ITERATIONS 4// 4 iterations, sorting 8 bits per iteration

layout (local_size_x = WORKGROUP_SIZE) in;

uniform uint g_num_elements;

layout (std430, set = 0, binding = 0) buffer elements_in {
    uint g_elements_in[];
};

layout (std430, set = 0, binding = 1) buffer elements_out {
    uint g_elements_out[];
};

layout (std430, set = 0, binding = 2) buffer indices_in {
    uint g_indices_in[];
};

layout (std430, set = 0, binding = 3) buffer indices_out {
    uint g_indices_out[];
};

shared uint[RADIX_SORT_BINS] histogram;
shared uint[RADIX_SORT_BINS / SUBGROUP_SIZE] sums;// subgroup reductions
shared uint[RADIX_SORT_BINS] local_offsets;// local exclusive scan (prefix sum) (inside subgroups)
shared uint[RADIX_SORT_BINS] global_offsets;// global exclusive scan (prefix sum)

struct BinFlags {
    uint flags[WORKGROUP_SIZE / BITS];
};
shared BinFlags[RADIX_SORT_BINS] bin_flags;

#define ELEMENT_IN(index, iteration) (iteration % 2 == 0 ? g_elements_in[index] : g_elements_out[index])
#define INDEX_IN(index, iteration) (iteration % 2 == 0 ? g_indices_in[index] : g_indices_out[index])

void main() {
    uint lID = gl_LocalInvocationID.x;
    uint sID = gl_SubgroupID;
    uint lsID = gl_SubgroupInvocationID;

    for (uint iteration = 0; iteration < ITERATIONS; iteration++) {
        uint shift = 8 * iteration;

        // initialize histogram
        if (lID < RADIX_SORT_BINS) {
            histogram[lID] = 0U;
        }
        barrier();

        for (uint ID = lID; ID < g_num_elements; ID += WORKGROUP_SIZE) {
            // determine the bin
            const uint bin = (ELEMENT_IN(ID, iteration) >> shift) & (RADIX_SORT_BINS - 1);
            // increment the histogram
            atomicAdd(histogram[bin], 1U);
        }
        barrier();

        // subgroup reductions and subgroup prefix sums
        if (lID < RADIX_SORT_BINS) {
            uint histogram_count = histogram[lID];
            uint sum = subgroupAdd(histogram_count);
            uint prefix_sum = subgroupExclusiveAdd(histogram_count);
            local_offsets[lID] = prefix_sum;
            if (subgroupElect()) {
                // one thread inside the warp/subgroup enters this section
                sums[sID] = sum;
            }
        }
        barrier();

        // global prefix sums (offsets)
        if (sID == 0) {
            uint offset = 0;
            for (uint i = lsID; i < RADIX_SORT_BINS; i += SUBGROUP_SIZE) {
                global_offsets[i] = offset + local_offsets[i];
                offset += sums[i / SUBGROUP_SIZE];
            }
        }
        barrier();

        //     ==== scatter keys according to global offsets =====
        const uint flags_bin = lID / BITS;
        const uint flags_bit = 1 << (lID % BITS);

        for (uint blockID = 0; blockID < g_num_elements; blockID += WORKGROUP_SIZE) {
            barrier();

            const uint ID = blockID + lID;

            // initialize bin flags
            if (lID < RADIX_SORT_BINS) {
                for (int i = 0; i < WORKGROUP_SIZE / BITS; i++) {
                    bin_flags[lID].flags[i] = 0U;// init all bin flags to 0
                }
            }
            barrier();

            uint element_in = 0;
            uint index_in = 0;
            uint binID = 0;
            uint binOffset = 0;
            if (ID < g_num_elements) {
                element_in = ELEMENT_IN(ID, iteration);
                index_in = INDEX_IN(ID, iteration);
                binID = (element_in >> shift) & uint(RADIX_SORT_BINS - 1);
                // offset for group
                binOffset = global_offsets[binID];
                // add bit to flag
                atomicAdd(bin_flags[binID].flags[flags_bin], flags_bit);
            }
            barrier();

            if (ID < g_num_elements) {
                // calculate output index of element
                uint prefix = 0;
                uint count = 0;
                for (uint i = 0; i < WORKGROUP_SIZE / BITS; i++) {
                    const uint bits = bin_flags[binID].flags[i];
                    const uint full_count = bitCount(bits);
                    const uint partial_count = bitCount(bits & (flags_bit - 1));
                    prefix += (i < flags_bin) ? full_count : 0U;
                    prefix += (i == flags_bin) ? partial_count : 0U;
                    count += full_count;
                }
                if (iteration % 2 == 0) {
                    g_elements_out[binOffset + prefix] = element_in;
                    g_indices_out[binOffset + prefix] = index_in;
                } else {
                    g_elements_in[binOffset + prefix] = element_in;
                    g_indices_in[binOffset + prefix] = index_in;
                }
                if (prefix == count - 1) {
                    atomicAdd(global_offsets[binID], count);
                }
            }
        }
    }
}
