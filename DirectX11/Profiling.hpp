#pragma once


#include <d3d11_1.h>
#include <string>
#include <Windows.h>

// We need to have __d3d11_h__ defined before nvapi.h so that it can pick up optional
// calls in the nvapi.  It is defined by d3d11.h.
#include "nvapi.h"

namespace profiling {
    enum class mode {
        none = 0,
        summary,
        top_command_lists,
        top_commands,
        cto_warning,

        invalid, // Must be last
    };

    class Overhead {
    public:
        LARGE_INTEGER cpu;
        unsigned count, hits;

        void clear();
    };

    struct State {
        LARGE_INTEGER start_time;
    };

    inline void start(State *state);
    inline void end(State *state, profiling::Overhead *overhead);

    template<class T>
    static inline typename T::iterator lookup_map(T &map, typename T::key_type key, profiling::Overhead *overhead)
    {
        profiling::State state;

        if (profiling::profile_type == profiling::mode::summary) {
            overhead->count++;
            profiling::start(&state);
        }
        auto ret = map.find(key);
        if (profiling::profile_type == profiling::mode::summary) {
            profiling::end(&state, overhead);
            if (ret != end(map))
                overhead->hits++;
        }
        return ret;
    }

    void update_txt();
    void update_cto_warning(bool warn);
    void clear();

    extern mode profile_type;
    extern Overhead present_overhead;
    extern Overhead overlay_overhead;
    extern Overhead draw_overhead;
    extern Overhead map_overhead;
    extern Overhead hash_tracking_overhead;
    extern Overhead stat_overhead;
    extern Overhead shaderregex_overhead;
    extern Overhead cursor_overhead;
    extern Overhead nvapi_overhead;
    extern std::wstring text;
    extern std::wstring cto_warning;
    extern INT64 interval;
    extern bool freeze;

    extern Overhead shader_hash_lookup_overhead;
    extern Overhead shader_reload_lookup_overhead;
    extern Overhead shader_original_lookup_overhead;
    extern Overhead shaderoverride_lookup_overhead;
    extern Overhead texture_handle_info_lookup_overhead;
    extern Overhead textureoverride_lookup_overhead;
    extern Overhead resource_pool_lookup_overhead;

    extern unsigned resource_full_copies;
    extern unsigned resource_reference_copies;
    extern unsigned inter_device_copies;
    extern unsigned stereo2mono_copies;
    extern unsigned msaa_resolutions;
    extern unsigned buffer_region_copies;
    extern unsigned views_cleared;
    extern unsigned resources_created;
    extern unsigned resource_pool_swaps;
    extern unsigned max_copies_per_frame_exceeded;
    extern unsigned injected_draw_calls;
    extern unsigned skipped_draw_calls;
    extern unsigned max_executions_per_frame_exceeded;
    extern unsigned iniparams_updates;

    // NvAPI profiling:

    inline NVAPI_INTERFACE NvAPI_Stereo_Enable(void);
    inline NVAPI_INTERFACE NvAPI_Stereo_IsEnabled(NvU8* pIsStereoEnabled);
    inline NVAPI_INTERFACE NvAPI_Stereo_IsActivated(StereoHandle stereoHandle, NvU8* pIsStereoOn);
    inline NVAPI_INTERFACE NvAPI_Stereo_GetEyeSeparation(StereoHandle hStereoHandle, float* pSeparation);
    inline NVAPI_INTERFACE NvAPI_Stereo_GetSeparation(StereoHandle stereoHandle, float* pSeparationPercentage);
    inline NVAPI_INTERFACE NvAPI_Stereo_SetSeparation(StereoHandle stereoHandle, float newSeparationPercentage);
    inline NVAPI_INTERFACE NvAPI_Stereo_GetConvergence(StereoHandle stereoHandle, float* pConvergence);
    inline NVAPI_INTERFACE NvAPI_Stereo_SetConvergence(StereoHandle stereoHandle, float newConvergence);
    inline NVAPI_INTERFACE NvAPI_Stereo_SetActiveEye(StereoHandle hStereoHandle, NV_STEREO_ACTIVE_EYE StereoEye);
    inline NVAPI_INTERFACE NvAPI_Stereo_ReverseStereoBlitControl(StereoHandle hStereoHandle, NvU8 TurnOn);
    inline NVAPI_INTERFACE NvAPI_Stereo_SetSurfaceCreationMode(__in StereoHandle hStereoHandle, __in NVAPI_STEREO_SURFACECREATEMODE creationMode);
    inline NVAPI_INTERFACE NvAPI_Stereo_GetSurfaceCreationMode(__in StereoHandle hStereoHandle, __in NVAPI_STEREO_SURFACECREATEMODE* pCreationMode);
    inline NVAPI_INTERFACE NvAPI_DISP_GetDisplayConfig(__inout NvU32* pathInfoCount, __out_ecount_full_opt(*pathInfoCount) NV_DISPLAYCONFIG_PATH_INFO* pathInfo);
#if defined(_D3D9_H_) || defined(__d3d10_h__) || defined(__d3d11_h__)
    inline NVAPI_INTERFACE NvAPI_D3D_GetCurrentSLIState(IUnknown* pDevice, NV_GET_CURRENT_SLI_STATE* pSliState);
#endif

}
