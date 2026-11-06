# A test program that loads the built plug-in and drives it like a host. The tests don't link any of the
# plug-in's code: they only use one of its headers, for the parameter IDs.

add_executable(DiffusionDelayTests
    TestMain.cpp
    Tests.h
    Harness.h
    Harness.cpp
    CompatibilityTests.cpp
    EchoTests.cpp
)

target_include_directories(DiffusionDelayTests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(DiffusionDelayTests PRIVATE hadronyche_testhost)
add_dependencies(DiffusionDelayTests DiffusionDelay)

# It drives a real window with Windows' mouse messages.
if(WIN32)
    target_sources(DiffusionDelayTests PRIVATE PanelTest.cpp)
endif()

set(DIFFUSIONDELAY_TESTS
    class-ids parameter-ids parameter-ranges saved-format buses latency-report
    clean-echo echo-time-accuracy feedback-decay endless-clean
    reflections-around-the-repeat tail-longer-than-reflections reflections-follow-time stereo-scatter mono-balance
    endless-diffused mass-builds-up
    damper-shelf damper-first-repeat-clean damper-endless
    sync-time sync-follows-tempo sync-off-ignores-tempo sync-without-tempo sync-stays-in-range
    time-glide held-note-stays-bounded dc-offset broken-input extreme-settings sample-rates
    dry-wet volume block-sizes determinism silence-flags parameter-state parameter-text
    panel panel-opens
)

hadronyche_add_host_tests(DiffusionDelayTests DiffusionDelay ${DIFFUSIONDELAY_TESTS})
