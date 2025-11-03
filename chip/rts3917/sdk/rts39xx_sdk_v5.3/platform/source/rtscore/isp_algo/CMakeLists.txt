cmake_minimum_required(VERSION 2.8)

project(isp_algo NONE)

if (${ENABLE_AE_ALGO})
    set(algos ${algos} librts_algo_ae.so)
endif()
if (${ENABLE_AWB_ALGO})
    set(algos ${algos} librts_algo_awb.so)
endif()
if (${ENABLE_AF_ALGO})
    set(algos ${algos} librts_algo_af.so)
endif()
set(algos ${algos} librts_algo_other.so)

install(FILES ${algos} DESTINATION lib/rtsisp/algos
        PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
