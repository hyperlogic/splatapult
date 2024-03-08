/**
* VkRadixSort written by Mirco Werner: https://github.com/MircoWerner/VkRadixSort
* Based on implementation of Intel's Embree: https://github.com/embree/embree/blob/v4.0.0-ploc/kernels/rthwif/builder/gpu/sort.h
*/
#version 460
//#extension GL_GOOGLE_include_directive: enable

#define WORKGROUP_SIZE 256 // assert WORKGROUP_SIZE >= RADIX_SORT_BINS
#define RADIX_SORT_BINS 256

uniform uint g_num_elements;
uniform uint g_shift;
//uniform uint g_num_workgroups;
uniform uint g_num_blocks_per_workgroup;

layout (local_size_x = WORKGROUP_SIZE) in;

layout (std430, binding = 0) buffer elements_in {
    uint g_elements_in[];
};

layout (std430, binding = 1) buffer histograms {
    // [histogram_of_workgroup_0 | histogram_of_workgroup_1 | ... ]
    uint g_histograms[]; // |g_histograms| = RADIX_SORT_BINS * #WORKGROUPS
};

shared uint[RADIX_SORT_BINS] histogram;

void main() {
    uint gID = gl_GlobalInvocationID.x;
    uint lID = gl_LocalInvocationID.x;
    uint wID = gl_WorkGroupID.x;

    // initialize histogram
    if (lID < RADIX_SORT_BINS) {
        histogram[lID] = 0U;
    }
    barrier();

    for (uint index = 0; index < g_num_blocks_per_workgroup; index++) {
        uint elementId = wID * g_num_blocks_per_workgroup * WORKGROUP_SIZE + index * WORKGROUP_SIZE + lID;
        if (elementId < g_num_elements) {
            // determine the bin
            const uint bin = (g_elements_in[elementId] >> g_shift) & (RADIX_SORT_BINS - 1);
            // increment the histogram
            atomicAdd(histogram[bin], 1U);
        }
    }
    barrier();

    if (lID < RADIX_SORT_BINS) {
        g_histograms[RADIX_SORT_BINS * wID + lID] = histogram[lID];
    }
}
