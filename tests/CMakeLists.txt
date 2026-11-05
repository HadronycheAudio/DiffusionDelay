# A test program that loads the built plug-in and drives it like a host. The tests don't link any of the
# plug-in's code: they only use one of its headers, for the parameter IDs.

add_executable(DiffusionDelayTests
    TestMain.cpp
    Tests.h
    Harness.h
    Harness.cpp
    CompatibilityTests.cpp
    EchoTests.cpp
    PanelTest.cpp
    "${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/coreiids.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/ustring.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/common/memorystream.cpp"
)

target_include_directories(DiffusionDelayTests PRIVATE "${VST3_SDK_DIR}" "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(DiffusionDelayTests PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(DiffusionDelayTests PRIVATE user32 gdi32)
add_dependencies(DiffusionDelayTests DiffusionDelay)

# One CTest test per test in the program, so that a single one can be run with ctest -R.
set(DIFFUSIONDELAY_TESTS
    class-ids parameter-ids parameter-ranges saved-format buses latency-report
    clean-echo echo-time-accuracy feedback-decay endless-clean
    reflections-around-the-repeat tail-longer-than-reflections reflections-follow-time stereo-scatter mono-balance
    endless-diffused mass-builds-up
    damper-shelf damper-first-repeat-clean damper-endless
    sync-time sync-follows-tempo sync-off-ignores-tempo sync-without-tempo sync-stays-in-range
    time-glide held-note-stays-bounded dc-offset broken-input extreme-settings sample-rates
    dry-wet volume block-sizes determinism silence-flags parameter-state parameter-text
    panel
)

foreach(test_name ${DIFFUSIONDELAY_TESTS})
    add_test(NAME ${test_name} COMMAND DiffusionDelayTests "$<TARGET_FILE:DiffusionDelay>" ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
endforeach()
